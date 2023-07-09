#include "esp_log.h"
//#include "mbcontroller.h"
#include "epever.h"


typedef struct
{
    uint16_t uint16_value[50];
} discrete_reg_params_t;

typedef struct
{
    uint16_t uint16_value[50];
} coil_reg_params_t;

typedef struct
{
    uint16_t uint16_value[50];
} input_reg_params_t;

typedef struct
{
    uint16_t uint16_value[50];
} holding_reg_params_t;

holding_reg_params_t holding_reg_params;
input_reg_params_t input_reg_params;
coil_reg_params_t coil_reg_params;
discrete_reg_params_t discrete_reg_params;



#define MB_PORT_NUM     (CONFIG_MB_UART_PORT_NUM)   // Number of UART port used for Modbus connection
#define MB_DEV_SPEED    (CONFIG_MB_UART_BAUD_RATE)  // The communication speed of the UART

// Note: Some pins on target chip cannot be assigned for UART communication.
// See UART documentation for selected board and target to configure pins using Kconfig.

// The number of parameters that intended to be used in the particular control process
#define MASTER_MAX_CIDS num_device_parameters

// Number of reading of parameters from slave
#define MASTER_MAX_RETRY 30

// Timeout to update cid over Modbus
#define UPDATE_CIDS_TIMEOUT_MS          (500)
#define UPDATE_CIDS_TIMEOUT_TICS        (UPDATE_CIDS_TIMEOUT_MS / portTICK_RATE_MS)

// Timeout between polls
#define POLL_TIMEOUT_MS                 (1)
#define POLL_TIMEOUT_TICS               (POLL_TIMEOUT_MS / portTICK_RATE_MS)

#define MASTER_TAG "MASTER_TEST"


#define MASTER_CHECK(a, ret_val, str, ...) \
    if (!(a)) { \
        ESP_LOGE(MASTER_TAG, "%s(%u): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        return (ret_val); \
    }

// The macro to get offset for parameter in the appropriate structure
#define HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_t, field) + 1))
#define INPUT_OFFSET(field) ((uint16_t)(offsetof(input_reg_params_t, field) + 1))
#define COIL_OFFSET(field) ((uint16_t)(offsetof(coil_reg_params_t, field) + 1))
// Discrete offset macro
#define DISCR_OFFSET(field) ((uint16_t)(offsetof(discrete_reg_params_t, field) + 1))

#define STR(fieldname) ((const char*)( fieldname ))
// Options can be used as bit masks or parameter limits
#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }

// Enumeration of modbus device addresses accessed by master device
enum {
    MB_CC_ADDR1 = 1, 
    MB_CC_ADDR2 = 2, 
    MB_INVERTER_ADDR = 3
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum {

    InverterLoad = 0,
    InverterOnoff,
    InverterLocalorRemote,

    CC2Realtime1,
    CC2Realtime2,

    CC2Statistical1,
    CC2Statistical2,

    CC1Rated1 ,
    CC1Rated2,

    CC1Realtime1,
    CC1Realtime2,
    CC1Realtime3,
    CC1Realtime4,
    CC1Realtime5,

    CC1Statistical1,
    CC1Statistical2,

    CC1Setting1,
    CC1Setting2,
    CC1Setting3,
    CC1Setting4,
    CC1Setting5,
    CC1Setting6,
    CC1Setting7,
    CC1Setting8,

    CC1Coil1,
    CC1Coil2,

    CC1CC1Discrete1,
    CC1CC1Discrete2,

};






const mb_parameter_descriptor_t device_parameters[] = {
    // { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}
    {InverterLoad, STR("InverterLoad"), STR(""), MB_INVERTER_ADDR, MB_PARAM_INPUT, 0x3108, 8, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 16, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {InverterOnoff, STR("InverterOnoff"), STR(""), MB_INVERTER_ADDR, MB_PARAM_COIL, 0xF, 1, 
        COIL_OFFSET(uint16_value), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0), PAR_PERMS_READ },       
    {InverterLocalorRemote, STR("InverterLocalorRemote"), STR(""), MB_INVERTER_ADDR, MB_PARAM_COIL, 0x11, 1, 
        COIL_OFFSET(uint16_value), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0), PAR_PERMS_READ },

    {CC2Realtime1, STR("CC2Realtime1"), STR(""), MB_CC_ADDR2, MB_PARAM_INPUT, 0x3100, 8, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 16, OPTS( 0, 0, 0), PAR_PERMS_READ },   
    {CC2Realtime2, STR("CC2Realtime2"), STR(""), MB_CC_ADDR2, MB_PARAM_INPUT, 0x310c, 7,
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 14, OPTS( 0, 0, 0), PAR_PERMS_READ },

    {CC2Statistical1, STR("Statistical21"), STR(""), MB_CC_ADDR2, MB_PARAM_INPUT, 0x3300, 22, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 44, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC2Statistical2, STR("Statistical22"), STR(""), MB_CC_ADDR2, MB_PARAM_INPUT, 0x331b, 4, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 8, OPTS( 0, 0, 0), PAR_PERMS_READ },

    {CC1Rated1, STR("CC1Rated1"), STR(""), MB_CC_ADDR1, MB_PARAM_INPUT, 0x3000, 9, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 18, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Rated2, STR("CC1Rated2"), STR(""), MB_CC_ADDR1, MB_PARAM_INPUT, 0x300e, 1, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 2, OPTS( 0, 65535, 1), PAR_PERMS_READ },

    {CC1Realtime1, STR("CC1Realtime1"), STR(""), MB_CC_ADDR1, MB_PARAM_INPUT, 0x3100, 8, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 16, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Realtime2, STR("CC1Realtime2"), STR(""), MB_CC_ADDR1, MB_PARAM_INPUT, 0x310c, 7,
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 14, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Realtime3, STR("CC1Realtime3"), STR(""), MB_CC_ADDR1, MB_PARAM_INPUT, 0x311a, 2, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 4, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Realtime4, STR("CC1Realtime4"), STR(""), MB_CC_ADDR1, MB_PARAM_INPUT, 0x311d, 1, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Realtime5, STR("CC1Realtime5"), STR(""), MB_CC_ADDR1, MB_PARAM_INPUT, 0x3200, 2, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 4, OPTS( 0, 0, 0), PAR_PERMS_READ },

    {CC1Statistical1, STR("Statistical11"), STR(""), MB_CC_ADDR1, MB_PARAM_INPUT, 0x3300, 22, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 44, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Statistical2, STR("Statistical12"), STR(""), MB_CC_ADDR1, MB_PARAM_INPUT, 0x331b, 4, 
        INPUT_OFFSET(uint16_value), PARAM_TYPE_U16, 8, OPTS( 0, 0, 0), PAR_PERMS_READ },

    {CC1Setting1, STR("CC1Setting1"), STR(""), MB_CC_ADDR1, MB_PARAM_HOLDING, 0x9000, 15, 
        HOLD_OFFSET(uint16_value), PARAM_TYPE_U16, 30, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Setting2, STR("CC1Setting2"), STR(""), MB_CC_ADDR1, MB_PARAM_HOLDING, 0x9013, 15, 
        HOLD_OFFSET(uint16_value), PARAM_TYPE_U16, 30, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Setting3, STR("CC1Setting3"), STR(""), MB_CC_ADDR1, MB_PARAM_HOLDING, 0x903d, 3, 
        HOLD_OFFSET(uint16_value), PARAM_TYPE_U16, 6, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Setting4, STR("CC1Setting4"), STR(""), MB_CC_ADDR1, MB_PARAM_HOLDING, 0x9042, 12, 
        HOLD_OFFSET(uint16_value), PARAM_TYPE_U16, 24, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Setting5, STR("CC1Setting5"), STR(""), MB_CC_ADDR1, MB_PARAM_HOLDING, 0x9065, 1, 
        HOLD_OFFSET(uint16_value), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Setting6, STR("CC1Setting6"), STR(""), MB_CC_ADDR1, MB_PARAM_HOLDING, 0x9067, 1, 
        HOLD_OFFSET(uint16_value), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Setting7, STR("CC1Setting7"), STR(""), MB_CC_ADDR1, MB_PARAM_HOLDING, 0x9069, 6, 
        HOLD_OFFSET(uint16_value), PARAM_TYPE_U16, 14, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Setting8, STR("CC1Setting8"), STR(""), MB_CC_ADDR1, MB_PARAM_HOLDING, 0x9070, 1, 
        HOLD_OFFSET(uint16_value), PARAM_TYPE_U16, 14, OPTS( 0, 0, 0), PAR_PERMS_READ },

    {CC1Coil1, STR("CC1Coil1"), STR(""), MB_CC_ADDR1, MB_PARAM_COIL, 0x2, 1, 
        COIL_OFFSET(uint16_value), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1Coil2, STR("CC1Coil2"), STR(""), MB_CC_ADDR1, MB_PARAM_COIL, 0x5, 2, 
        COIL_OFFSET(uint16_value), PARAM_TYPE_U16, 4, OPTS( 0, 0, 0), PAR_PERMS_READ },

    {CC1CC1Discrete1, STR("CC1Discrete1"), STR(""), MB_CC_ADDR1, MB_PARAM_DISCRETE, 0x2000, 1, 
        DISCR_OFFSET(uint16_value), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0), PAR_PERMS_READ },
    {CC1CC1Discrete2, STR("CC1Discrete2"), STR(""), MB_CC_ADDR1, MB_PARAM_DISCRETE, 0x200c, 1, 
        DISCR_OFFSET(uint16_value), PARAM_TYPE_U16, 2, OPTS( 0, 0, 0), PAR_PERMS_READ },
};


// Calculate number of parameters in the table
const uint16_t num_device_parameters = (sizeof(device_parameters)/sizeof(device_parameters[0]));

// The function to get pointer to parameter storage (instance) according to parameter description table
static void* master_get_param_data(const mb_parameter_descriptor_t* param_descriptor)
{
    assert(param_descriptor != NULL);
    void* instance_ptr = NULL;
    if (param_descriptor->param_offset != 0) {
       switch(param_descriptor->mb_param_type)
       {
           case MB_PARAM_HOLDING:
               instance_ptr = ((void*)&holding_reg_params + param_descriptor->param_offset - 1);
               break;
           case MB_PARAM_INPUT:
               instance_ptr = ((void*)&input_reg_params + param_descriptor->param_offset - 1);
               break;
           case MB_PARAM_COIL:
               instance_ptr = ((void*)&coil_reg_params + param_descriptor->param_offset - 1);
               break;
           case MB_PARAM_DISCRETE:
               instance_ptr = ((void*)&discrete_reg_params + param_descriptor->param_offset - 1);
               break;
           default:
               instance_ptr = NULL;
               break;
       }
    } else {
        ESP_LOGE(MASTER_TAG, "Wrong parameter offset for CID #%d", param_descriptor->cid);
        assert(instance_ptr != NULL);
    }
    return instance_ptr;
}



// Modbus master initialization
esp_err_t master_init(void)
{
    // Initialize and start Modbus controller
    mb_communication_info_t comm = {
            .port = MB_PORT_NUM,
            .mode = MB_MODE_RTU,
            .baudrate = MB_DEV_SPEED,
            .parity = MB_PARITY_NONE
    };
    void* master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MASTER_CHECK((master_handler != NULL), ESP_ERR_INVALID_STATE,
                                "mb controller initialization fail.");
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            "mb controller initialization fail, returns(0x%x).",
                            (uint32_t)err);
    err = mbc_master_setup((void*)&comm);
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            "mb controller setup fail, returns(0x%x).",
                            (uint32_t)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, CONFIG_MB_UART_TXD, CONFIG_MB_UART_RXD,
                              CONFIG_MB_UART_RTS, UART_PIN_NO_CHANGE);

    err = mbc_master_start();
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                            "mb controller start fail, returns(0x%x).",
                            (uint32_t)err);

    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
            "mb serial set pin failure, uart_set_pin() returned (0x%x).", (uint32_t)err);
    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
            "mb serial set mode failure, uart_set_mode() returned (0x%x).", (uint32_t)err);

    vTaskDelay(5);
    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MASTER_CHECK((err == ESP_OK), ESP_ERR_INVALID_STATE,
                                "mb controller set descriptor fail, returns(0x%x).",
                                (uint32_t)err);
    ESP_LOGI(MASTER_TAG, "Modbus master stack initialized...");
    return err;
}


void master_read_load_inverter(volatile epever_load_inverter_t *epever)
{
    ESP_LOGI(MASTER_TAG,"Begin reading Load....");
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;
    ESP_LOGI(MASTER_TAG,"InverterLoad");
    err = mbc_master_get_cid_info(InverterLoad, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[8] = { 0 };
                err = mbc_master_get_parameter(InverterLoad, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->LoadInputVoltage= *(uint16_t*)&value[0];
                    epever->LoadInputCurrent = *(uint16_t*)&value[1];
                    epever->LoadInputPower = (*(uint16_t*)&value[2]) | (*(uint16_t*)&value[3]) << 16;

                    epever->LoadOutputVoltage = *(uint16_t*)&value[4];
                    epever->LoadOutputCurrent = *(uint16_t*)&value[5];
                    //epever->LoadOutputPower = (*(uint16_t*)&value[7]) | (*(uint16_t*)&value[6]) << 16;
                    epever->LoadOutputPower = (*(uint16_t*)&value[6]) | (*(uint16_t*)&value[7]) << 16;
                    //epever->LoadOutputPower = (*(uint16_t*)&value[6]) ;//<< 16;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls

    ESP_LOGI(MASTER_TAG,"End reading Load.");

}
void master_read_status_inverter(volatile epever_status_inverter_t *epever)
{
    ESP_LOGI(MASTER_TAG,"Begin master_read_status_inverter....");
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    ESP_LOGI(MASTER_TAG,"InverterLocalorRemote");
    err = mbc_master_get_cid_info(InverterLocalorRemote, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[2] = { 0 };
                err = mbc_master_get_parameter(InverterLocalorRemote, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->InverterLocalorRemote = (*(uint16_t*)&value[0]) > 1;
                    //(uint16_t)value[0];
                    // read decimal 2 when remote 0 when local
                    //    uint16_t InverterLocalorRemote; //1 Remote control 0 Local control
                    //epever->ManualControlTheLoad = (*(uint16_t*)&value[0]) > 1;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"InverterOnoff");
    err = mbc_master_get_cid_info(InverterOnoff, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0; 
                uint16_t value[2] = { 0 };
                err = mbc_master_get_parameter(InverterOnoff, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->InverterOnoff =(*(uint16_t*)&value[0]) > 1;
                    // read decimal 128 when on 0 when off
                    //1 Turn on the inverter output 0 Turn off the Inverter output
                    // (uint16_t)value[0];
                    //epever->ForceTheLoadOnOff = (*(uint16_t*)&value[1]) > 1;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"End master_read_status_inverter.");
}


void master_set_status_inverter(bool value)
{ 
    ESP_LOGI(MASTER_TAG,"Begin master_set_status_inverter") ;
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;
    uint8_t type = 0;
    //uint8_t val2 = 0xF;
    uint8_t val2 = 0x3;
    err = mbc_master_get_cid_info(InverterLocalorRemote, &param_descriptor);
    err = mbc_master_set_parameter(InverterLocalorRemote, (char*)param_descriptor->param_key, (uint8_t*)&val2, &type);
    if (err == ESP_OK) {
        ESP_LOGI(MASTER_TAG,"set local remote ok.");
    }else{
        ESP_LOGI(MASTER_TAG,"set local remote fail.");
    }
    vTaskDelay(UPDATE_CIDS_TIMEOUT_TICS);
    param_descriptor = NULL;
    type = 0;
    err = mbc_master_get_cid_info(InverterOnoff, &param_descriptor);
    if (value == 1) 
    {
        val2 = 0x81;
    }
    else 
    {
        val2 = 0;
    }
    err = mbc_master_set_parameter(InverterOnoff, (char*)param_descriptor->param_key, (uint8_t*)&val2, &type);
    if (err == ESP_OK) {
        ESP_LOGI(MASTER_TAG,"set inverter on off ok.");
    }else{
        ESP_LOGI(MASTER_TAG,"set inverter on off fail.");
    }
    
    vTaskDelay(UPDATE_CIDS_TIMEOUT_TICS);
    ESP_LOGI(MASTER_TAG,"End master_set_status_inverter.");
}

void master_read_rated_cc(volatile epever_rated_cc_t *epever)
{
    ESP_LOGI(MASTER_TAG,"Begin reading....");
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;


    ESP_LOGI(MASTER_TAG,"CC1Rated1");
    err = mbc_master_get_cid_info(CC1Rated1, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[9] = { 0 };
                err = mbc_master_get_parameter(CC1Rated1, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->ChargingRatedInputVoltage = *(uint16_t*)&value[0];
                    epever->ChargingRatedInputCurrent = *(uint16_t*)&value[1];
                    epever->ChargingRatedInputPower = (*(uint16_t*)&value[2]) | (*(uint16_t*)&value[3]) << 16;

                    epever->ChargingRatedOutputVoltage = *(uint16_t*)&value[4];
                    epever->ChargingRatedOutputCurrent = *(uint16_t*)&value[5];
                    epever->ChargingRatedOutputPower = (*(uint16_t*)&value[6]) | (*(uint16_t*)&value[7]) << 16;

                    epever->ChargingMode  = *(uint16_t*)&value[8];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls


    ESP_LOGI(MASTER_TAG,"CC1Rated2");
    err = mbc_master_get_cid_info(CC1Rated2, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value = 0;
                err = mbc_master_get_parameter(CC1Rated2, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->OutputRatedLoadCurrent = *(uint16_t*)&value;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls

    ESP_LOGI(MASTER_TAG,"End Rated reading.");
}

void master_read_realtime_cc1(volatile epever_realtime_cc_t *epever)
{
    ESP_LOGI(MASTER_TAG,"Begin reading Realtime.1...");
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    ESP_LOGI(MASTER_TAG,"CC1Realtime1");
    err = mbc_master_get_cid_info(CC1Realtime1, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[8] = { 0 };
                err = mbc_master_get_parameter(CC1Realtime1, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->ChargingInputVoltage= *(uint16_t*)&value[0];
                    epever->ChargingInputCurrent = *(uint16_t*)&value[1];
                    epever->ChargingInputPower = (*(uint16_t*)&value[2]) | (*(uint16_t*)&value[3]) << 16;

                    epever->ChargingOutputVoltage = *(uint16_t*)&value[4];
                    epever->ChargingOutputCurrent = *(uint16_t*)&value[5];
                    epever->ChargingOutputPower = (*(uint16_t*)&value[6]) | (*(uint16_t*)&value[7]) << 16;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls

    ESP_LOGI(MASTER_TAG,"CC1Realtime2");
    err = mbc_master_get_cid_info(CC1Realtime2, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[7] = { 0 };
                err = mbc_master_get_parameter(CC1Realtime2, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->DischargingOutputVoltage= *(uint16_t*)&value[0];
                    epever->DischargingOutputCurrent = *(uint16_t*)&value[1];
                    epever->DischargingOutputPower = (*(uint16_t*)&value[2]) | (*(uint16_t*)&value[3]) << 16;

                    epever->BatteryTemperature = *(uint16_t*)&value[4];
                    epever->ChargerCaseTemperature = *(uint16_t*)&value[5];
                    epever->ChargerSiliconTemperature = *(uint16_t*)&value[6];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls
/*
    ESP_LOGI(MASTER_TAG,"CC1Realtime3");
    err = mbc_master_get_cid_info(CC1Realtime3, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[2] = { 0 };
                err = mbc_master_get_parameter(CC1Realtime3, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->BatterySOC= *(uint16_t*)&value[0];
                    epever->RemoteBatteryTemperature = *(uint16_t*)&value[1];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls


    ESP_LOGI(MASTER_TAG,"CC1Realtime4");
    err = mbc_master_get_cid_info(CC1Realtime4, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value = 0 ;
                err = mbc_master_get_parameter(CC1Realtime4, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->BatteryRealRatedVoltage= *(uint16_t*)&value;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls

    ESP_LOGI(MASTER_TAG,"CC1Realtime5");
    err = mbc_master_get_cid_info(CC1Realtime5, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[2] = { 0 };
                err = mbc_master_get_parameter(CC1Realtime5, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->BatteryStatus= *(uint16_t*)&value[0];
                    epever->ChargerStatus = *(uint16_t*)&value[1];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls
*/
    ESP_LOGI(MASTER_TAG,"End reading Realtime 1.");

    
}

void master_read_realtime_cc2(volatile epever_realtime_cc_t *epever)
{

    ESP_LOGI(MASTER_TAG,"Begin reading Realtime 2....");
    
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    ESP_LOGI(MASTER_TAG,"CC2Realtime1");
    err = mbc_master_get_cid_info(CC2Realtime1, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[8] = { 0 };
                err = mbc_master_get_parameter(CC2Realtime1, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->ChargingInputVoltage= *(uint16_t*)&value[0];
                    epever->ChargingInputCurrent = *(uint16_t*)&value[1];
                    epever->ChargingInputPower = (*(uint16_t*)&value[2]) | (*(uint16_t*)&value[3]) << 16;

                    epever->ChargingOutputVoltage = *(uint16_t*)&value[4];
                    epever->ChargingOutputCurrent = *(uint16_t*)&value[5];
                    epever->ChargingOutputPower = (*(uint16_t*)&value[6]) | (*(uint16_t*)&value[7]) << 16;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls
    ESP_LOGI(MASTER_TAG,"CC2Realtime2");
    err = mbc_master_get_cid_info(CC2Realtime2, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[7] = { 0 };
                err = mbc_master_get_parameter(CC2Realtime2, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->DischargingOutputVoltage= *(uint16_t*)&value[0];
                    epever->DischargingOutputCurrent = *(uint16_t*)&value[1];
                    epever->DischargingOutputPower = (*(uint16_t*)&value[2]) | (*(uint16_t*)&value[3]) << 16;

                    epever->BatteryTemperature = *(uint16_t*)&value[4];
                    epever->ChargerCaseTemperature = *(uint16_t*)&value[5];
                    epever->ChargerSiliconTemperature = *(uint16_t*)&value[6];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls
    ESP_LOGI(MASTER_TAG,"End reading Realtime 2.");
   
}


void master_read_statistical_cc1(volatile epever_statistical_cc_t *epever)
{
ESP_LOGI(MASTER_TAG,"Begin reading Statistical..1..");
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

ESP_LOGI(MASTER_TAG,"Statistical11");
    err = mbc_master_get_cid_info(CC1Statistical1, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[22] = { 0 };
                err = mbc_master_get_parameter(CC1Statistical1, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->TodayMaxInputVoltage = *(uint16_t*)&value[0];
                    epever->TodayMinInputVoltage = *(uint16_t*)&value[1];
                    epever->TodayMaxBatteryVoltage = *(uint16_t*)&value[2];
                    epever->TodayMinBatteryVoltage = *(uint16_t*)&value[3];

                    epever->TodayConsumedEnergy = (*(uint16_t*)&value[4]) | (*(uint16_t*)&value[5]) << 16;
                    epever->ThisMonthConsumedEnergy = (*(uint16_t*)&value[6]) | (*(uint16_t*)&value[7]) << 16;
                    epever->ThisYearConsumedEnergy = (*(uint16_t*)&value[8]) | (*(uint16_t*)&value[9]) << 16;
                    epever->TotalConsumedEnergy = (*(uint16_t*)&value[10]) | (*(uint16_t*)&value[11]) << 16;

                    epever->TodayGeneratedEnergy = (*(uint16_t*)&value[12]) | (*(uint16_t*)&value[13]) << 16;
                    epever->ThisMonthGeneratedEnergy = (*(uint16_t*)&value[14]) | (*(uint16_t*)&value[15]) << 16;
                    epever->ThisYearGeneratedEnergy = (*(uint16_t*)&value[16]) | (*(uint16_t*)&value[17]) << 16;
                    epever->TotalGeneratedEnergy = (*(uint16_t*)&value[18]) | (*(uint16_t*)&value[19]) << 16;

                    epever->CarbonDioxideReduction = (*(uint16_t*)&value[20]) | (*(uint16_t*)&value[21]) << 16;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls

    ESP_LOGI(MASTER_TAG,"Statistical12");
    err = mbc_master_get_cid_info(CC1Statistical2, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[4] = { 0 };
                err = mbc_master_get_parameter(CC1Statistical2, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->BatteryCurrent = (*(int32_t*)&value[0]) | (*(uint16_t*)&value[1]) << 16;
                    epever->BatteryTemperature = *(uint16_t*)&value[2];
                    epever->AmbientTemperature = *(uint16_t*)&value[3];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls

    ESP_LOGI(MASTER_TAG,"End reading Statistical.1.");
}

void master_read_statistical_cc2(volatile epever_statistical_cc_t *epever)
{
ESP_LOGI(MASTER_TAG,"Begin reading Statistical..2..");
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

ESP_LOGI(MASTER_TAG,"Statistical21");
    err = mbc_master_get_cid_info(CC1Statistical1, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[22] = { 0 };
                err = mbc_master_get_parameter(CC1Statistical1, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->TodayMaxInputVoltage = *(uint16_t*)&value[0];
                    epever->TodayMinInputVoltage = *(uint16_t*)&value[1];
                    epever->TodayMaxBatteryVoltage = *(uint16_t*)&value[2];
                    epever->TodayMinBatteryVoltage = *(uint16_t*)&value[3];

                    epever->TodayConsumedEnergy = (*(uint16_t*)&value[4]) | (*(uint16_t*)&value[5]) << 16;
                    epever->ThisMonthConsumedEnergy = (*(uint16_t*)&value[6]) | (*(uint16_t*)&value[7]) << 16;
                    epever->ThisYearConsumedEnergy = (*(uint16_t*)&value[8]) | (*(uint16_t*)&value[9]) << 16;
                    epever->TotalConsumedEnergy = (*(uint16_t*)&value[10]) | (*(uint16_t*)&value[11]) << 16;

                    epever->TodayGeneratedEnergy = (*(uint16_t*)&value[12]) | (*(uint16_t*)&value[13]) << 16;
                    epever->ThisMonthGeneratedEnergy = (*(uint16_t*)&value[14]) | (*(uint16_t*)&value[15]) << 16;
                    epever->ThisYearGeneratedEnergy = (*(uint16_t*)&value[16]) | (*(uint16_t*)&value[17]) << 16;
                    epever->TotalGeneratedEnergy = (*(uint16_t*)&value[18]) | (*(uint16_t*)&value[19]) << 16;

                    epever->CarbonDioxideReduction = (*(uint16_t*)&value[20]) | (*(uint16_t*)&value[21]) << 16;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls

    ESP_LOGI(MASTER_TAG,"Statistical22");
    err = mbc_master_get_cid_info(CC1Statistical2, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[4] = { 0 };
                err = mbc_master_get_parameter(CC1Statistical2, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->BatteryCurrent = (*(int32_t*)&value[0]) | (*(uint16_t*)&value[1]) << 16;
                    epever->BatteryTemperature = *(uint16_t*)&value[2];
                    epever->AmbientTemperature = *(uint16_t*)&value[3];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS); // timeout between polls

    ESP_LOGI(MASTER_TAG,"End reading Statistical.2.");
}



void master_read_setting_cc(volatile epever_setting_cc_t *epever)
{
    ESP_LOGI(MASTER_TAG,"Begin reading Setting....");
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    ESP_LOGI(MASTER_TAG,"CC1Setting1");
    err = mbc_master_get_cid_info(CC1Setting1, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[15] = { 0 };
                err = mbc_master_get_parameter(CC1Setting1, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->BatteryType = *(uint16_t*)&value[0];
                    epever->BatteryCapacity = *(uint16_t*)&value[1];
                    epever->TemperatureCompensationCoefficient = *(uint16_t*)&value[2];
                    epever->HighVoltDisconnect = *(uint16_t*)&value[3];
                    epever->ChargingLimitVoltage = *(uint16_t*)&value[4];
                    epever->OverVoltageReconnect = *(uint16_t*)&value[5];
                    epever->EqualizationVoltage = *(uint16_t*)&value[6];
                    epever->BoostVoltage = *(uint16_t*)&value[7];
                    epever->FloatVoltage = *(uint16_t*)&value[8];
                    epever->BoostReconnectVoltage = *(uint16_t*)&value[9];
                    epever->LowVoltageReconnect = *(uint16_t*)&value[10];
                    epever->UnderVoltageRecover = *(uint16_t*)&value[11];
                    epever->UnderVoltageWarning = *(uint16_t*)&value[12];
                    epever->LowVoltageDisconnect = *(uint16_t*)&value[13];
                    epever->DischargingLimitVoltage = *(uint16_t*)&value[14];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    
    ESP_LOGI(MASTER_TAG,"CC1Setting2");
    err = mbc_master_get_cid_info(CC1Setting2, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[15] = { 0 };
                err = mbc_master_get_parameter(CC1Setting2, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    //(*(uint16_t*)&value[0]) | (*(uint16_t*)&value[0]) << 16;
                    //((value[2] & 0xFF) * 100) + ((value[2] >> 8) & 0xFF)
                    epever->RealTimeClock1 = *(uint16_t*)&value[0];
                    epever->RealTimeClock2 = *(uint16_t*)&value[1];
                    epever->RealTimeClock3 = *(uint16_t*)&value[2];
                    
                    epever->RealTimeClock1 = ((value[0] & 0xFF) * 100) + ((value[0] >> 8) & 0xFF);
                    epever->RealTimeClock2 = ((value[1] & 0xFF) * 100) + ((value[1] >> 8) & 0xFF);
                    epever->RealTimeClock3 = ((value[2] & 0xFF) * 100) + ((value[2] >> 8) & 0xFF);
                    //epever->RealTimeClocktm = (struct tm){.tm_year=(2000+(value[2] >> 8) & 0xFF)-1900, .tm_mon = ((value[2] & 0xFF))-1, .tm_mday = ((value[1] >> 8) & 0xFF), .tm_hour = ((value[1] & 0xFF)) , .tm_min = ((value[0] >> 8) & 0xFF) , .tm_sec = ((value[0] & 0xFF))};
                    epever->RealTimeClocktm = (struct tm){.tm_year=((value[2] >> 8) & 0xFF)+100, .tm_mon = ((value[2] & 0xFF))-1, .tm_mday = ((value[1] >> 8) & 0xFF), .tm_hour = ((value[1] & 0xFF)) , .tm_min = ((value[0] >> 8) & 0xFF) , .tm_sec = ((value[0] & 0xFF))};
                    epever->RealTimeClock = (time_t)mktime (&(struct tm){.tm_year=((value[2] >> 8) & 0xFF)+100, .tm_mon = ((value[2] & 0xFF))-1, .tm_mday = ((value[1] >> 8) & 0xFF), .tm_hour = ((value[1] & 0xFF)) , .tm_min = ((value[0] >> 8) & 0xFF) , .tm_sec = ((value[0] & 0xFF))});
                    //epever->RealTimeClock = mktime ((struct tm){.tm_year=((value[2] >> 8) & 0xFF), .tm_mon = ((value[2] & 0xFF)), .tm_mday = ((value[1] >> 8) & 0xFF), .tm_hour = ((value[1] & 0xFF)) , .tm_min = ((value[0] >> 8) & 0xFF) , .tm_sec = ((value[0] & 0xFF))});
                    epever->EqualizationChargingCycle = *(uint16_t*)&value[3];
                    epever->BatteryTemperatureWarningUpperLimit = *(uint16_t*)&value[4];
                    epever->BatteryTemperatureWarningLowerLimit = *(uint16_t*)&value[5];
                    epever->ControllerInnerTemperatureUpperLimit = *(uint16_t*)&value[6];
                    epever->ControllerInnerTemperatureUpperLimitRecover = *(uint16_t*)&value[7];
                    epever->PowerComponentTemperatureUpperLimit = *(uint16_t*)&value[8];
                    epever->PowerComponentTemperatureUpperLimitRecover = *(uint16_t*)&value[9];
                    epever->LineImpedance = *(uint16_t*)&value[10];
                    epever->NightTimeThresholdVolt = *(uint16_t*)&value[11];
                    epever->LightSignalStartupDelayTime = *(uint16_t*)&value[12];
                    epever->DayTimeThresholdVolt = *(uint16_t*)&value[13];
                    epever->LightSignalTurnOffDelayTime = *(uint16_t*)&value[14];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"CC1Setting3");
    err = mbc_master_get_cid_info(CC1Setting3, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[3] = { 0 };
                err = mbc_master_get_parameter(CC1Setting3, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->LoadControlingMode = *(uint16_t*)&value[0];
                    epever->WorkingTimeLength1 = *(uint16_t*)&value[1];
                    epever->WorkingTimeLength2 = *(uint16_t*)&value[2];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"CC1Setting4");
    err = mbc_master_get_cid_info(CC1Setting4, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[12] = { 0 };
                err = mbc_master_get_parameter(CC1Setting4, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->TurnOnTiming1Second = *(uint16_t*)&value[0];
                    epever->TurnOnTiming1Minute = *(uint16_t*)&value[1];
                    epever->TurnOnTiming1Hour = *(uint16_t*)&value[2];
                    epever->TurnOffTiming1Second = *(uint16_t*)&value[3];
                    epever->TurnOffTiming1Minute = *(uint16_t*)&value[4];
                    epever->TurnOffTiming1Hour = *(uint16_t*)&value[5];
                    epever->TurnOnTiming2Second = *(uint16_t*)&value[6];
                    epever->TurnOnTiming2Minute = *(uint16_t*)&value[7];
                    epever->TurnOnTiming2Hour = *(uint16_t*)&value[8];
                    epever->TurnOffTiming2Second = *(uint16_t*)&value[9];
                    epever->TurnOffTiming2Minute = *(uint16_t*)&value[10];
                    epever->TurnOffTiming2Hour = *(uint16_t*)&value[11];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"CC1Setting5");
    err = mbc_master_get_cid_info(CC1Setting5, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[1] = { 0 } ;
                err = mbc_master_get_parameter(CC1Setting5, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->LengthOfNight = *(uint16_t*)&value[0];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"CC1Setting6");
    err = mbc_master_get_cid_info(CC1Setting6, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[1] = { 0 };
                err = mbc_master_get_parameter(CC1Setting6, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->BatteryRatedVoltageCode = *(uint16_t*)&value[0];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"CC1Setting7");
    err = mbc_master_get_cid_info(CC1Setting7, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[6] = { 0 };
                err = mbc_master_get_parameter(CC1Setting7, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->LoadTimingControlSelection = *(uint16_t*)&value[0];
                    epever->DefaultLoadOnOffInManualMode = *(uint16_t*)&value[1];
                    epever->EqualizeDuration = *(uint16_t*)&value[2];
                    epever->BoostDuration = *(uint16_t*)&value[3];
                    epever->DischargingPercentage = *(uint16_t*)&value[4];
                    epever->ChargingPercentage = *(uint16_t*)&value[5];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"CC1Setting8");
    err = mbc_master_get_cid_info(CC1Setting8, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[1] = { 0 };
                err = mbc_master_get_parameter(CC1Setting8, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->ManagementModeOfBatteryChargingAndDischarging = *(uint16_t*)&value[0];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"End reading Setting.");
}


void master_read_coil_cc(volatile epever_coil_cc_t *epever)
{
    ESP_LOGI(MASTER_TAG,"Begin reading coil....");
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    ESP_LOGI(MASTER_TAG,"CC1Coil1");
    err = mbc_master_get_cid_info(CC1Coil1, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[1] = { 0 };
                err = mbc_master_get_parameter(CC1Coil1, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->ManualControlTheLoad = (*(uint16_t*)&value[0]) > 1;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"CC1Coil2");
    err = mbc_master_get_cid_info(CC1Coil2, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[2] = { 0 };
                err = mbc_master_get_parameter(CC1Coil2, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->EnableLoadTestMode = (*(uint16_t*)&value[0]) > 1;
                    epever->ForceTheLoadOnOff = (*(uint16_t*)&value[1]) > 1;
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"End reading coil.");
}


void master_read_discrete_cc(volatile epever_discrete_cc_t *epever)
{
    ESP_LOGI(MASTER_TAG,"Begin reading discrete....");
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    ESP_LOGI(MASTER_TAG,"CC1Discrete1");
    err = mbc_master_get_cid_info(CC1CC1Discrete1, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[1] = { 0 };
                err = mbc_master_get_parameter(CC1CC1Discrete1, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->OverTemperatureInsideTheDevice = *(uint16_t*)&value[0];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"CC1Discrete2");
    err = mbc_master_get_cid_info(CC1CC1Discrete2, &param_descriptor);
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                uint16_t value[1] = { 0 };
                err = mbc_master_get_parameter(CC1CC1Discrete2, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                if (err == ESP_OK) {
                    epever->DayNight = *(uint16_t*)&value[0];
                }
        }
    }
    vTaskDelay(POLL_TIMEOUT_TICS);

    ESP_LOGI(MASTER_TAG,"End reading discrete.");
}



void master_write_coil_cc(bool value)
{ 
    ESP_LOGI(MASTER_TAG,"Begin master_write_coil") ;
    esp_err_t err = ESP_OK;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    err = mbc_master_get_cid_info(CC1Coil1, &param_descriptor);
    uint8_t type = 0;
    err = mbc_master_set_parameter(CC1Coil1, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
    if (err == ESP_OK) {
        ESP_LOGI(MASTER_TAG,"Write coil ok.");
    }else{
        ESP_LOGI(MASTER_TAG,"Write coil fail.");
    }
    /*
    if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
        if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                //void* temp_data_ptr = master_get_param_data(param_descriptor);
                //assert(temp_data_ptr);
                
        }
    }*/
                                                              

    vTaskDelay(POLL_TIMEOUT_TICS);
    ESP_LOGI(MASTER_TAG,"End write coil.");
}

