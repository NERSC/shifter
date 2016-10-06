#!/usr/bin/env python

import subprocess
import os
import sys
import re


class TestGPUSupportException(Exception):
    pass

def test_log(text):
    print "test GPU support: %s" % text

def test_expect(actual, expected):
    if actual != expected:
        message = "found {}, but expcted {}".format(actual, expected)
        raise TestGPUSupportException(message)

def get_nvidia_devices(image, cmdline_gpus=None, environment_variable_gpus=None):
    environment = os.environ.copy()
    if cmdline_gpus is not None:
        environment["CUDA_VISIBLE_DEVICES"] = environment_variable_gpus
   
    command = ["shifter", "--image="+image]
    if cmdline_gpus is not None:
        commanand += ["--gpu="+cmdline_gpus]
    command += ["ls", "/dev"]

    out = subprocess.check_output(command, env=environment)
    expr = re.compile('nvidia[0-9]+')
    return [line for line in out.split('\n') if expr.match(line) is not None]

if __name__ == "__main__":
    image_name = sys.argv[1]

    test_log("create NVIDIA devices if they don't exit yet")
    subprocess.call(["sudo", "touch", "/dev/nvidia0"])
    subprocess.call(["sudo", "touch", "/dev/nvidia1"])

    test_log("no CUDA_VISIBLE_DEVICES + no --gpu option ==> the container cannot see any nvidia device")
    devices = get_nvidia_devices(image_name)
    test_expect(len(devices), 0)

    test_log("--gpu=0 ==> the container can only see /dev/nvidia0")
    devices = get_nvidia_devices(image_name, cmdline_gpus="0")
    test_expect(devices, ["nvidia0"])

    test_log("--gpu=1 ==> the container can only see /dev/nvidia1")
    devices = get_nvidia_devices(image_name, cmdline_gpus="1")
    test_expect(devices, ["nvidia1"])

    test_log("CUDA_VISIBLE_DEVICES=0 ==> the container can only see /dev/nvidia0")
    devices = get_nvidia_devices(image_name, environment_variable_gpus="0")
    test_expect(devices, ["nvidia0"])

    test_log("CUDA_VISIBLE_DEVICES=1 ==> the container can only see /dev/nvidia1")
    devices = get_nvidia_devices(image_name, environment_variable_gpus="0")
    test_expect(devices, ["nvidia1"])

    test_log("CUDA_VISIBLE_DEVICES overrides --gpu")
    devices = get_nvidia_devices(image_name, cmdline_gpus="1", environment_variable_gpus="0")
    test_expect(devices, ["nvidia0"])

