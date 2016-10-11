import unittest
import subprocess
import os
import sys
import re

_container_image=None

class TestGPUSupport(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        subprocess.call(["sudo", "touch", "/dev/nvidia0"])
        subprocess.call(["sudo", "touch", "/dev/nvidia1"])

    #no CUDA_VISIBLE_DEVICES + no --gpu option ==> container can see no GPU
    def test_no_environment_variable_and_no_command_line_option(self):
        devices = self._get_nvidia_devices()
        self.assertEqual(devices, [])

    #--gpu=0 ==> container can see /dev/nvidia0"
    def test_activate_gpu0_with_command_line_option(self):
        devices = self._get_nvidia_devices(cmdline_gpus="0")
        self.assertEqual(devices, ["nvidia0"])

    #--gpu=1 ==> container can see /dev/nvidia1"
    def test_activate_gpu1_with_command_line_option(self):
        devices = self._get_nvidia_devices(cmdline_gpus="1")
        self.assertEqual(devices, ["nvidia1"])

    #--gpu=0,1 ==> container can see both GPUs
    def test_activate_gpu0_and_gpu1_with_command_line_option(self):
        devices = self._get_nvidia_devices(cmdline_gpus="0,1")
        self.assertEqual(devices, ["nvidia0", "nvidia1"])

    #CUDA_VISIBLE_DEVICES= ==> container can see no GPU
    def test_deactivate_gpus_with_environment_variable(self):
        devices = self._get_nvidia_devices(environment_variable_gpus="")
        self.assertEqual(devices, [])

    #CUDA_VISIBLE_DEVICES=0 ==> container can see /dev/nvidia0
    def test_activate_gpu0_with_environment_variable(self):
        devices = self._get_nvidia_devices(environment_variable_gpus="0")
        self.assertEqual(devices, ["nvidia0"])

    #CUDA_VISIBLE_DEVICES=1 ==> container can see /dev/nvidia1
    def test_activate_gpu0_with_environment_variable(self):
        devices = self._get_nvidia_devices(environment_variable_gpus="1")
        self.assertEqual(devices, ["nvidia1"])

    #CUDA_VISIBLE_DEVICES=0,1 ==> container can see both GPUs
    def test_activate_gpu0_and_gpu1_with_environment_variable(self):
        devices = self._get_nvidia_devices(environment_variable_gpus="0,1")
        self.assertEqual(devices, ["nvidia0", "nvidia1"])

    #CUDA_VISIBLE_DEVICES= and --gpu=0,1 ==> container can see no GPU
    def test_environment_variable_overrides_command_line_option_0(self):
        devices = self._get_nvidia_devices(cmdline_gpus="0,1", environment_variable_gpus="")
        self.assertEqual(devices, [])

    #CUDA_VISIBLE_DEVICES=0 and --gpu=0,1 ==> container can see /dev/nvidia0
    def test_environment_variable_overrides_command_line_option_1(self):
        devices = self._get_nvidia_devices(cmdline_gpus="0,1", environment_variable_gpus="0")
        self.assertEqual(devices, ["nvidia0"])

    def _get_nvidia_devices(self, cmdline_gpus=None, environment_variable_gpus=None):
        environment = os.environ.copy()
        if environment_variable_gpus is not None:
            environment["CUDA_VISIBLE_DEVICES"] = environment_variable_gpus

        command = ["shifter", "--image="+_container_image]
        if cmdline_gpus is not None:
            command += ["--gpu="+cmdline_gpus]
        command += ["ls", "/dev"]

        out = subprocess.check_output(command, env=environment)
        expr = re.compile('nvidia[0-9]+')
        return [line for line in out.split('\n') if expr.match(line) is not None]

if __name__ == "__main__":
    _container_image=sys.argv[1]
    del sys.argv[1]
    unittest.main()

