import unittest
import subprocess
import os
import sys
import re

_container_image=None


class TestGPUDevices(unittest.TestCase):
    """
    These tests verify that the GPU resources and configurations (device files,
    libraries and value of LD_LIBRARY_PATH) are correctly set inside the container.
    """

    _GPU_DEVICES = {"nvidia0", "nvidia1"}
    _GPU_LIBS = {   "libcuda.so", \
                    "libnvcuvid.so", \
                    "libnvidia-compiler.so", \
                    "libnvidia-encode.so", \
                    "libnvidia-ml.so", \
                    "libnvidia-fatbinaryloader.so"}
    _GPU_BINS = {   "nvidia-cuda-mps-control", \
                    "nvidia-cuda-mps-server", \
                    "nvidia-debugdump", \
                    "nvidia-persistenced", \
                    "nvidia-smi"}
    _GPU_ENV_LD_LIB_PATH = {"/gpu-support/nvidia/lib", "/gpu-support/nvidia/lib64"}
    _GPU_ENV_PATH = {"/gpu-support/nvidia/bin"}

    _created_gpu_devices = set()
    _created_gpu_libs = set()
    _created_gpu_bins = set()

    @classmethod
    def setUpClass(cls):
        cls._create_gpu_devices()
        cls._create_gpu_libraries()
        cls._create_gpu_binaries()

    @classmethod
    def tearDownClass(cls):
        cls._remove_gpu_devices()
        cls._remove_gpu_libraries()
        cls._remove_gpu_binaries()

    @classmethod
    def _create_gpu_devices(cls):
        for gpu_device in cls._GPU_DEVICES:
            if not os.path.exists("/dev/" + gpu_device):
                cls._created_gpu_devices.add(gpu_device)
                subprocess.call(["sudo", "touch", "/dev/" + gpu_device])

    @classmethod
    def _remove_gpu_devices(cls):
        for gpu_device in cls._created_gpu_devices:
            subprocess.call(["sudo", "rm", "/dev/" + gpu_device])

    @classmethod
    def _create_gpu_libraries(cls):
        path = os.path.dirname(os.path.abspath(__file__))
        for gpu_lib in cls._GPU_LIBS:
            if not os.path.exists("/lib/" + gpu_lib):
                cls._created_gpu_libs.add(gpu_lib)
                subprocess.call(["sudo", "cp", path + "/gpu_support_dummy_64bit_library.so", "/lib/" + gpu_lib])
        subprocess.call(["sudo", "ldconfig"])

    @classmethod
    def _remove_gpu_libraries(cls):
        for gpu_lib in cls._created_gpu_libs:
            subprocess.call(["sudo", "rm", "/lib/" + gpu_lib])
        subprocess.call(["sudo", "ldconfig"])

    @classmethod
    def _create_gpu_binaries(cls):
        for gpu_bin in cls._GPU_BINS:
            if not os.path.exists("/bin/" + gpu_bin):
                cls._created_gpu_bins.add(gpu_bin)
                subprocess.call(["sudo", "touch", "/bin/" + gpu_bin])
                subprocess.call(["sudo", "chmod", "755", "/bin/" + gpu_bin])

    @classmethod
    def _remove_gpu_binaries(cls):
        for gpu_bin in cls._created_gpu_bins:
            subprocess.call(["sudo", "rm", "/bin/" + gpu_bin])

    def setUp(self):
        self.cmdline_gpus=None
        self.environment_variable_gpus=None

    #no CUDA_VISIBLE_DEVICES + no --gpu option ==> container can see no GPU
    def test_no_environment_variable_and_no_command_line_option(self):
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, set())
        self.assertEqual(libs, set())
        self.assertEqual(bins, set())
        self.assertEqual(env_ld_lib_path, set())
        self.assertEqual(env_path, set())

    #--gpu=0 ==> container can see /dev/nvidia0"
    def test_activate_gpu0_with_command_line_option(self):
        self.cmdline_gpus="0"
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, {"nvidia0"})
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins)
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)

    #--gpu=1 ==> container can see /dev/nvidia1"
    def test_activate_gpu1_with_command_line_option(self):
        self.cmdline_gpus="1"
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, {"nvidia1"})
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins) 
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)

    #--gpu=0,1 ==> container can see both GPUs
    def test_activate_gpu0_and_gpu1_with_command_line_option(self):
        self.cmdline_gpus="0,1"
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, {"nvidia0", "nvidia1"})
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins)
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)

    #CUDA_VISIBLE_DEVICES= ==> container can see no GPU
    def test_deactivate_gpus_with_environment_variablei_0(self):
        self.environment_variable_gpus=""
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, set())
        self.assertEqual(libs, set())
        self.assertEqual(bins, set())
        self.assertEqual(env_ld_lib_path, set())
        self.assertEqual(env_path, set())
    
    #CUDA_VISIBLE_DEVICES=NoDevFiles ==> container can see no GPU
    def test_deactivate_gpus_with_environment_variable_1(self):
        self.environment_variable_gpus="NoDevFiles"
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, set())
        self.assertEqual(libs, set())
        self.assertEqual(libs, set())
        self.assertEqual(env_ld_lib_path, set())
        self.assertEqual(env_path, set())

    #CUDA_VISIBLE_DEVICES=0 ==> container can see /dev/nvidia0
    def test_activate_gpu0_with_environment_variable(self):
        self.environment_variable_gpus="0"
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, {"nvidia0"})
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins)
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)

    #CUDA_VISIBLE_DEVICES=1 ==> container can see /dev/nvidia1
    def test_activate_gpu0_with_environment_variable(self):
        self.environment_variable_gpus="1"
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, {"nvidia1"})
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins)
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)

    #CUDA_VISIBLE_DEVICES=0,1 ==> container can see both GPUs
    def test_activate_gpu0_and_gpu1_with_environment_variable(self):
        self.environment_variable_gpus="0,1"
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, {"nvidia0", "nvidia1"})
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins)
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)


    #CUDA_VISIBLE_DEVICES= and --gpu=0,1 ==> container can see no GPU
    def test_environment_variable_overrides_command_line_option_0(self):
        self.cmdline_gpus="0,1"
        self.environment_variable_gpus=""
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, set())
        self.assertEqual(libs, set())
        self.assertEqual(bins, set())
        self.assertEqual(env_ld_lib_path, set())
        self.assertEqual(env_path, set())
    
    #CUDA_VISIBLE_DEVICES=NoDevFiles and --gpu=0,1 ==> container can see no GPU
    def test_environment_variable_overrides_command_line_option_1(self):
        self.cmdline_gpus="0,1"
        self.environment_variable_gpus="NoDevFiles"
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, set())
        self.assertEqual(libs, set())
        self.assertEqual(bins, set())
        self.assertEqual(env_ld_lib_path, set())
        self.assertEqual(env_path, set())

    #CUDA_VISIBLE_DEVICES=0 and --gpu=0,1 ==> container can see /dev/nvidia0
    def test_environment_variable_overrides_command_line_option_2(self):
        self.cmdline_gpus="0,1"
        self.environment_variable_gpus="0"
        devices, libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(devices, {"nvidia0"})
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins)
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)

    def _get_gpu_properties_in_container(self):
        return (self._get_gpu_devices_in_container(), \
                self._get_gpu_libraries_in_container(), \
                self._get_gpu_binaries_in_container(), \
                self._get_gpu_paths_in_container_environment_variable("LD_LIBRARY_PATH"), \
                self._get_gpu_paths_in_container_environment_variable("PATH"))

    def _get_gpu_devices_in_container(self):
        devices = self._get_command_output_in_container(["ls", "/dev"])
        expr = re.compile('nvidia[0-9]+')
        return set([device for device in devices if expr.match(device) is not None])

    def _get_gpu_libraries_in_container(self):
        if self._is_gpu_support_in_container_enabled():
            return set(self._get_command_output_in_container(["ls", "/gpu-support/nvidia/lib64"]))
        else:
            return set()

    def _get_gpu_binaries_in_container(self):
        if self._is_gpu_support_in_container_enabled():
            return set(self._get_command_output_in_container(["ls", "/gpu-support/nvidia/bin"]))
        else:
            return set()

    def _get_gpu_paths_in_container_environment_variable(self, variable_name):
        output = self._get_command_output_in_container(["env"])
        expr = re.compile(variable_name + "=")
        paths = None
        for out in output:
            if expr.match(out) is not None:
                paths = out.split("=")[1].split(":")
        expr = re.compile("/gpu-support")
        gpu_paths = {path for path in paths if expr.match(path) is not None}
        return gpu_paths

    def _is_gpu_support_in_container_enabled(self):
        return "gpu-support" in self._get_command_output_in_container(["ls", "/"])

    def _get_command_output_in_container(self, command):
        environment = os.environ.copy()
        if self.environment_variable_gpus is not None:
            environment["CUDA_VISIBLE_DEVICES"] = self.environment_variable_gpus

        full_command = ["shifter", "--image="+_container_image]
        if self.cmdline_gpus is not None:
            full_command += ["--gpu="+self.cmdline_gpus]
        full_command += command

        out = subprocess.check_output(full_command, env=environment)
        return [line for line in out.split('\n')]

    def _assert_is_subset(self, subset, superset):
        #dirty conversion to dictionaries required here
        #(there is no assertSetContainsSubset method)
        subset_dict = dict(zip(subset, len(subset)*[None]))
        superset_dict = dict(zip(superset, len(superset)*[None]))
        self.assertDictContainsSubset(subset_dict, superset_dict)

if __name__ == "__main__":
    _container_image=sys.argv[1]
    del sys.argv[1]
    unittest.main()
