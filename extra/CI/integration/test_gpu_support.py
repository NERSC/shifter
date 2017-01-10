import unittest
import subprocess
import os
import sys
import re

_container_image=None


class TestGPUSupport(unittest.TestCase):
    """
    These tests verify that the GPU resources and configurations (device files,
    libraries and value of LD_LIBRARY_PATH) are correctly set inside the container.
    """

    _GPU_DEVICES = {"nvidia0", "nvidia1"}
    _GPU_LIBS = {   "libcuda.so", \
                    "libnvidia-compiler.so", \
                    "libnvidia-ptxjitcompiler.so", \
                    "libnvidia-encode.so", \
                    "libnvidia-ml.so", \
                    "libnvidia-fatbinaryloader.so", \
                    "libnvidia-opencl.so" }
    _GPU_BINS = {   "nvidia-cuda-mps-control", \
                    "nvidia-cuda-mps-server", \
                    "nvidia-debugdump", \
                    "nvidia-persistenced", \
                    "nvidia-smi"}
    _GPU_ENV_LD_LIB_PATH = {"/opt/shifter/site-resources/gpu/lib", "/opt/shifter/site-resources/gpu/lib64"}
    _GPU_ENV_PATH = {"/opt/shifter/site-resources/gpu/bin"}

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
                cls._created_gpu_libs.add("/lib/" + gpu_lib)
                subprocess.call(["sudo", "cp", path + "/libdummy32bit.so", "/lib/" + gpu_lib])
            if not os.path.exists("/lib64/" + gpu_lib):
                cls._created_gpu_libs.add("/lib64/" + gpu_lib)
                subprocess.call(["sudo", "cp", path + "/libdummy64bit.so", "/lib64/" + gpu_lib])
        subprocess.call(["sudo", "sed", "-i", "/etc/ld.so.conf", "-e", "$a/lib64"]) # append /lib64 to ld.so.conf
        subprocess.call(["sudo", "ldconfig"])
        subprocess.call(["sudo", "sed", "-i", "/etc/ld.so.conf", "-e", "$d"]) # remove /lib64 from ld.so.conf

    @classmethod
    def _remove_gpu_libraries(cls):
        for gpu_lib in cls._created_gpu_libs:
            subprocess.call(["sudo", "rm", gpu_lib])
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
        self.cuda_visible_devices=None

    def test_cuda_visible_devices_doesnt_exist(self):
        libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(libs, set())
        self.assertEqual(bins, set())
        self.assertEqual(env_ld_lib_path, set())
        self.assertEqual(env_path, set())

    def test_cuda_visible_devices_is_empty(self):
        self.cuda_visible_devices=""
        libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(libs, set())
        self.assertEqual(bins, set())
        self.assertEqual(env_ld_lib_path, set())
        self.assertEqual(env_path, set())

    def test_cuda_visible_devices_is_nodevfiles(self):
        self.cuda_visible_devices="NoDevFiles"
        libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self.assertEqual(libs, set())
        self.assertEqual(libs, set())
        self.assertEqual(env_ld_lib_path, set())
        self.assertEqual(env_path, set())

    def test_cuda_visible_devices_is_0(self):
        self.cuda_visible_devices="0"
        libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins)
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)

    def test_cuda_visible_devices_is_1(self):
        self.cuda_visible_devices="1"
        libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins)
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)

    def test_cuda_visible_devices_is_0_1(self):
        self.cuda_visible_devices="0,1"
        libs, bins, env_ld_lib_path, env_path = self._get_gpu_properties_in_container()
        self._assert_is_subset(subset=self._GPU_LIBS, superset=libs)
        self._assert_is_subset(subset=self._GPU_BINS, superset=bins)
        self.assertEqual(self._GPU_ENV_LD_LIB_PATH, env_ld_lib_path)
        self.assertEqual(self._GPU_ENV_PATH, env_path)

    def _get_gpu_properties_in_container(self):
        return (self._get_gpu_libraries_in_container(),
                self._get_gpu_binaries_in_container(),
                self._get_gpu_paths_in_container_environment_variable("LD_LIBRARY_PATH"),
                self._get_gpu_paths_in_container_environment_variable("PATH"))

    def _get_gpu_libraries_in_container(self):
        if self._is_gpu_support_in_container_enabled():
            return set(self._get_command_output_in_container(["ls", "/opt/shifter/site-resources/gpu/lib64"]))
        else:
            return set()

    def _get_gpu_binaries_in_container(self):
        if self._is_gpu_support_in_container_enabled():
            return set(self._get_command_output_in_container(["ls", "/opt/shifter/site-resources/gpu/bin"]))
        else:
            return set()

    def _get_gpu_paths_in_container_environment_variable(self, variable_name):
        output = self._get_command_output_in_container(["env"])
        expr = re.compile(variable_name + "=")
        paths = []
        for out in output:
            if expr.match(out) is not None:
                paths = out.split("=")[1].split(":")
        expr = re.compile("/opt/shifter/site-resources/gpu")
        gpu_paths = {path for path in paths if expr.match(path) is not None}
        return gpu_paths

    def _is_gpu_support_in_container_enabled(self):
        return self._file_exists_in_container("/opt/shifter/site-resources/gpu")

    def _file_exists_in_container(self, file_path):
        command = ["bash", "-c", "if [ -e " + file_path + " ]; then echo \"file exists\"; fi"]
        out = self._get_command_output_in_container(command)
        return out == ["file exists"]

    def _get_command_output_in_container(self, command):
        environment = os.environ.copy()
        if self.cuda_visible_devices is not None:
            environment["CUDA_VISIBLE_DEVICES"] = self.cuda_visible_devices
        command = ["shifter", "--image="+_container_image] + command
        out = subprocess.check_output(command, env=environment)
        return self._command_output_without_trailing_new_lines(out)

    def _command_output_without_trailing_new_lines(self, out):
        lines = [line for line in out.split('\n')]
        # remove empty trailing elements in list (they are caused by trailing new lines)
        while len(lines)>0 and lines[-1]=="":
            lines.pop()
        return lines

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
