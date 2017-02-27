/**
 * @author Kean Mariotti <kean.mariotti@cscs.ch>
 */
#include <string>
#include <CppUTest/CommandLineTestRunner.h>
#include "gpu_support.h"

TEST_GROUP(GPUSupportTestGroup) {
};

TEST(GPUSupportTestGroup, parseGPUenv_test) {
    gpu_support_config config = {};

    // CUDA_VISIBLE_DEVICES doesn't exist in environment
    {
        parse_gpu_env(&config);
        CHECK(config.is_gpu_support_enabled == 0);
        CHECK(config.gpu_ids == NULL);
        free_gpu_support_config(&config);
    }
    // CUDA_VISIBLE_DEVICES= (no value)
    {
        setenv("CUDA_VISIBLE_DEVICES", "", 1);
        parse_gpu_env(&config);
        CHECK(config.is_gpu_support_enabled == 0);
        CHECK(config.gpu_ids == NULL);
        free_gpu_support_config(&config);
    }
    // CUDA_VISIBLE_DEVICES=NoDevFiles
    {
        setenv("CUDA_VISIBLE_DEVICES", "NoDevFiles", 1);
        parse_gpu_env(&config);
        CHECK(config.is_gpu_support_enabled == 0);
        CHECK(config.gpu_ids == NULL);
        free_gpu_support_config(&config);
    }
    // CUDA_VISIBLE_DEVICES=0
    {
        setenv("CUDA_VISIBLE_DEVICES", "0", 1);
        parse_gpu_env(&config);
        CHECK(config.is_gpu_support_enabled == 1);
        CHECK(config.gpu_ids == std::string("0"));
        free_gpu_support_config(&config);
    }
    // CUDA_VISIBLE_DEVICES=0,1
    {
        setenv("CUDA_VISIBLE_DEVICES", "0,1", 1);
        parse_gpu_env(&config);
        CHECK(config.is_gpu_support_enabled == 1);
        CHECK(config.gpu_ids == std::string("0,1"));
        free_gpu_support_config(&config);
    }
}

int main(int argc, char **argv) {
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
