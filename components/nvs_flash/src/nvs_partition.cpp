// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdlib>
#include "nvs_partition.hpp"
#include "stdio.h"

namespace nvs {

NVSPartition::NVSPartition(const esp_partition_t* partition)
    : mESPPartition(partition)
{
    // ensure the class is in a valid state
    if (partition == nullptr) {
        std::abort();
    }
}

const char *NVSPartition::get_partition_name()
{
    return mESPPartition->label;
}

esp_err_t NVSPartition::read_raw(size_t src_offset, void* dst, size_t size)
{
    return esp_partition_read_raw(mESPPartition, src_offset, dst, size);
}

esp_err_t NVSPartition::read(size_t src_offset, void* dst, size_t size)
{
    if (size % ESP_ENCRYPT_BLOCK_SIZE != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_partition_read(mESPPartition, src_offset, dst, size);
}

esp_err_t NVSPartition::write_raw(size_t dst_offset, const void* src, size_t size)
{
    uint8_t single_size = 10;
    uint32_t count = size / single_size;
    uint8_t count_leave = size % single_size;
    esp_err_t err = ESP_OK;
    for (uint32_t i = 0; i < count; i++) {
        err = esp_partition_write_raw(mESPPartition, dst_offset + i * single_size, (uint8_t *)src + i * single_size, single_size);
        if (err != ESP_OK) {
            return err;
        }
    }
    if (count_leave) {
        err = esp_partition_write_raw(mESPPartition, dst_offset + count * single_size, (uint8_t *)src + count * single_size, count_leave);
    }
    return err;
}

esp_err_t NVSPartition::write(size_t dst_offset, const void* src, size_t size)
{
    if (size % ESP_ENCRYPT_BLOCK_SIZE != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t single_size = 10;
    uint32_t count = size / single_size;
    uint8_t count_leave = size % single_size;
    esp_err_t err = ESP_OK;
    for (uint32_t i = 0; i < count; i++) {
        err = esp_partition_write(mESPPartition, dst_offset + i * single_size, (uint8_t *)src + i * single_size, single_size);
        if (err != ESP_OK) {
            return err;
        }
    }
    if (count_leave) {
        err = esp_partition_write(mESPPartition, dst_offset + count * single_size, (uint8_t *)src + count * single_size, count_leave);
    }
    return err;
}

esp_err_t NVSPartition::erase_range(size_t dst_offset, size_t size)
{
    return esp_partition_erase_range(mESPPartition, dst_offset, size);
}

uint32_t NVSPartition::get_address()
{
    return mESPPartition->address;
}

uint32_t NVSPartition::get_size()
{
    return mESPPartition->size;
}

} // nvs
