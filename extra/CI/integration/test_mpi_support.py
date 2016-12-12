import unittest
import subprocess
import os
import sys
import re

class TestMPISupport(unittest.TestCase):
    """
    These tests verify that the site MPI resources and configurations (libraries,
    binaries, configuration files, environment variables) are correctly
    set inside the container.
    """
    _created_site_resources = set()

    _SHIFTER_MPI_CONFIGURATION_FILE = "/usr/bin/activate_mpi_support.sh"
    #_SHIFTER_MPI_CONFIGURATION_FILE = "/opt/shifter/udiRoot/bin/activate_mpi_support.sh"

    _SITE_LIBS_PREFIX = os.path.dirname(os.path.abspath(__file__))
    _SITE_BINS_PREFIX = os.path.dirname(os.path.abspath(__file__))

    _SITE_MPI_LIBS = { "libmpi.so" : "libsitempi1.so.12.5.5", "libmpich.so" : "libsitempi2.so.12.5.5" }
    _SITE_MPI_DEPENDENCY_LIBS = { "libsitempidependency1.so", "libsitempidependency2.so" }
    _SITE_MPI_BINS = { "mpirun" }
    _SITE_MPI_CONFIGURATION_FILES = { "/etc/dummy-mpi-config-file" }

    _EXPECTED_CONTAINER_MPI_ENV_LD_LIB_PATH = "/site-resources/mpi/lib"
    _EXPECTED_CONTAINER_MPI_ENV_PATH = { "/site-resources/gpu/bin" }

    _CONTAINER_IMAGES = { \
        "mpich-compatible1" : "rukkal/shifter-mpi-support-test:mpich-compatible1", \
        "mpich-compatible2" : "rukkal/shifter-mpi-support-test:mpich-compatible2", \
        "mpich-compatible3" : "rukkal/shifter-mpi-support-test:mpich-compatible3", \
        "mpich-compatible4" : "rukkal/shifter-mpi-support-test:mpich-compatible4", \
        "mpich-incompatible1" : "rukkal/shifter-mpi-support-test:mpich-incompatible1", \
        "mpich-incompatible2" : "rukkal/shifter-mpi-support-test:mpich-incompatible2", \
        "mpich-incompatible3" : "rukkal/shifter-mpi-support-test:mpich-incompatible3", \
        "ld.so.cache-without-mpi-libraries" : "rukkal/shifter-mpi-support-test:ld.so.cache-without-mpi-libraries", }

    @classmethod
    def setUpClass(cls):
        cls._pull_docker_images()
        cls._create_site_resources()
        cls._modify_site_configuration_file()

    @classmethod
    def tearDownClass(cls):
        cls._undo_create_site_resources()
        cls._undo_modify_site_mpi_configuration_file()

    @classmethod
    def _pull_docker_images(cls):
        for container_image in cls._CONTAINER_IMAGES.values():
            subprocess.call(["shifterimg", "pull", container_image])

    @classmethod
    def _create_site_resources(cls):
        site_resources_path = os.path.dirname(os.path.abspath(__file__))
        dummy_lib_path = site_resources_path + "/libdummy64bit.so"
        # MPI libs
        for value in cls._SITE_MPI_LIBS.values():
            lib_path = site_resources_path + "/" + value
            subprocess.call(["cp", dummy_lib_path, lib_path])
            cls._created_site_resources.add(lib_path)
        # MPI dependency libs
        for value in cls._SITE_MPI_DEPENDENCY_LIBS:
            lib_path = site_resources_path + "/" + value
            subprocess.call(["cp", dummy_lib_path, lib_path])
            cls._created_site_resources.add(lib_path)
        # MPI bins
        for value in cls._SITE_MPI_BINS:
            bin_path = site_resources_path + "/" + value
            subprocess.call(["touch", bin_path])
            subprocess.call(["chmod", "755", bin_path])
            cls._created_site_resources.add(bin_path)
        # MPI config files
        for file_path in cls._SITE_MPI_CONFIGURATION_FILES:
            subprocess.call(["sudo", "touch", file_path])
            cls._created_site_resources.add(file_path)

    @classmethod
    def _undo_create_site_resources(cls):
        for resource in cls._created_site_resources:
            subprocess.call(["sudo", "rm", resource])

    @classmethod
    def _modify_site_configuration_file(cls):
        subprocess.call(["sudo", "cp", cls._SHIFTER_MPI_CONFIGURATION_FILE, cls._SHIFTER_MPI_CONFIGURATION_FILE + ".bak"])
        cls._modify_mpi_libs_in_site_configuration_file()
        cls._modify_mpi_dependency_libs_in_site_configuration_file()
        cls._modify_mpi_bins_in_site_configuration_file()
        cls._modify_mpi_config_files_in_site_configuration_file()
        print subprocess.check_output(["cat", cls._SHIFTER_MPI_CONFIGURATION_FILE])

    @classmethod
    def _modify_mpi_libs_in_site_configuration_file(cls):
        subprocess.call(["sudo", "sed", "-i", cls._SHIFTER_MPI_CONFIGURATION_FILE, "-e", "/^site_mpi_shared_libraries=/,/\"$/d"])
        sed_command = "1asite_mpi_shared_libraries=\""
        for key, value in cls._SITE_MPI_LIBS.items():
            sed_command += "\\n{}:{}".format(key, cls._SITE_LIBS_PREFIX + "/" + value)
        sed_command += "\""
        subprocess.call(["sudo", "sed", "-i", cls._SHIFTER_MPI_CONFIGURATION_FILE, "-e", sed_command])

    @classmethod
    def _modify_mpi_dependency_libs_in_site_configuration_file(cls):
        subprocess.call(["sudo", "sed", "-i", cls._SHIFTER_MPI_CONFIGURATION_FILE, "-e", "/^site_mpi_dependency_libraries=/,/\"$/d"])
        sed_command = "1asite_mpi_dependency_libraries=\""
        for value in cls._SITE_MPI_DEPENDENCY_LIBS:
            sed_command += "\\n{}".format(cls._SITE_LIBS_PREFIX + "/" + value)
        sed_command += "\""
        subprocess.call(["sudo", "sed", "-i", cls._SHIFTER_MPI_CONFIGURATION_FILE, "-e", sed_command])

    @classmethod
    def _modify_mpi_bins_in_site_configuration_file(cls):
        subprocess.call(["sudo", "sed", "-i", cls._SHIFTER_MPI_CONFIGURATION_FILE, "-e", "/^site_mpi_binaries=/,/\"$/d"])
        sed_command = "1asite_mpi_binaries=\""
        for value in cls._SITE_MPI_BINS:
            sed_command += "\\n{}".format(cls._SITE_BINS_PREFIX + "/" + value)
        sed_command += "\""
        subprocess.call(["sudo", "sed", "-i", cls._SHIFTER_MPI_CONFIGURATION_FILE, "-e", sed_command])

    @classmethod
    def _modify_mpi_config_files_in_site_configuration_file(cls):
        subprocess.call(["sudo", "sed", "-i", cls._SHIFTER_MPI_CONFIGURATION_FILE, "-e", "/^site_configuration_files=/,/\"$/d"])
        sed_command = "1asite_configuration_files=\""
        for value in cls._SITE_MPI_CONFIGURATION_FILES:
            sed_command += "\\n{}".format(value)
        sed_command += "\""
        subprocess.call(["sudo", "sed", "-i", cls._SHIFTER_MPI_CONFIGURATION_FILE, "-e", sed_command])

    @classmethod
    def _undo_modify_site_mpi_configuration_file(cls):
        subprocess.call(["sudo", "mv", cls._SHIFTER_MPI_CONFIGURATION_FILE + ".bak", cls._SHIFTER_MPI_CONFIGURATION_FILE])

    def setUp(self):
        self._mpi_command_line_option = None

    def test_no_mpi_support(self):
        self._mpi_command_line_option = False
        self._container_image = self._CONTAINER_IMAGES["mpich-compatible1"]
        properties = self._get_mpi_properties_in_container()
        self.assertEqual(properties["mpi-libs"], set())
        self.assertEqual(properties["mpi-dependency-libs"], set())
        self.assertEqual(properties["mpi-bins"], set())
        self.assertEqual(properties["mpi-config-files"], set())

    def test_mpich_compatible_1(self):
        self._mpi_command_line_option = True
        self._container_image = self._CONTAINER_IMAGES["mpich-compatible1"]
        properties = self._get_mpi_properties_in_container()
        self.assertEqual(properties["mpi-libs"], {"libmpi.so.12.5.5", "libmpich.so.12.5.5"})
        self.assertEqual(properties["mpi-dependency-libs"], self._SITE_MPI_DEPENDENCY_LIBS)
        self.assertEqual(properties["mpi-bins"], self._SITE_MPI_BINS)
        self.assertEqual(properties["mpi-config-files"], self._SITE_MPI_CONFIGURATION_FILES)

    def test_mpich_compatible_2(self):
        self._mpi_command_line_option = True
        self._container_image = self._CONTAINER_IMAGES["mpich-compatible2"]
        properties = self._get_mpi_properties_in_container()
        self.assertEqual(properties["mpi-libs"], {"libmpi.so.12.5.0", "libmpich.so.12.5.0"})
        self.assertEqual(properties["mpi-dependency-libs"], self._SITE_MPI_DEPENDENCY_LIBS)
        self.assertEqual(properties["mpi-bins"], self._SITE_MPI_BINS)
        self.assertEqual(properties["mpi-config-files"], self._SITE_MPI_CONFIGURATION_FILES)

    def test_mpich_compatible_3(self):
        self._mpi_command_line_option = True
        self._container_image = self._CONTAINER_IMAGES["mpich-compatible3"]
        properties = self._get_mpi_properties_in_container()
        self.assertEqual(properties["mpi-libs"], {"libmpi.so.12.5.10", "libmpich.so.12.5.10"})
        self.assertEqual(properties["mpi-dependency-libs"], self._SITE_MPI_DEPENDENCY_LIBS)
        self.assertEqual(properties["mpi-bins"], self._SITE_MPI_BINS)
        self.assertEqual(properties["mpi-config-files"], self._SITE_MPI_CONFIGURATION_FILES)

    def test_mpich_compatible_4(self):
        self._mpi_command_line_option = True
        self._container_image = self._CONTAINER_IMAGES["mpich-compatible4"]
        properties = self._get_mpi_properties_in_container()
        self.assertEqual(properties["mpi-libs"], {"libmpi.so.12.0.5", "libmpich.so.12.0.5"})
        self.assertEqual(properties["mpi-dependency-libs"], self._SITE_MPI_DEPENDENCY_LIBS)
        self.assertEqual(properties["mpi-bins"], self._SITE_MPI_BINS)
        self.assertEqual(properties["mpi-config-files"], self._SITE_MPI_CONFIGURATION_FILES)

    def test_mpich_incompatible_1(self):
        self._mpi_command_line_option = True
        self._container_image = self._CONTAINER_IMAGES["mpich-incompatible1"]
        self._assert_shifter_raises_mpi_error_containing_text(
            text = "Cannot find a site MPI library which is ABI-compatible with the container library")

    def test_mpich_incompatible_2(self):
        self._mpi_command_line_option = True
        self._container_image = self._CONTAINER_IMAGES["mpich-incompatible2"]
        self._assert_shifter_raises_mpi_error_containing_text(
            text = "Cannot find a site MPI library which is ABI-compatible with the container library")

    def test_mpich_incompatible_3(self):
        self._mpi_command_line_option = True
        self._container_image = self._CONTAINER_IMAGES["mpich-incompatible3"]
        self._assert_shifter_raises_mpi_error_containing_text(
            text = "Cannot find a site MPI library which is ABI-compatible with the container library")

    def test_ld_so_cache_without_mpi_libraries(self):
        self._mpi_command_line_option = True
        self._container_image = self._CONTAINER_IMAGES["ld.so.cache-without-mpi-libraries"]
        self._assert_shifter_raises_mpi_error_containing_text(
            text = "No MPI libraries found in the container")

    def _get_mpi_properties_in_container(self):
        return {"mpi-libs" : self._get_mpi_libs_in_container(), \
                "mpi-dependency-libs" : self._get_mpi_dependency_libs_in_container(), \
                "mpi-bins" : self._get_mpi_bins_in_container(), \
                "mpi-config-files" : self._get_mpi_config_files_in_container() }

    def _get_mpi_libs_in_container(self):
        site_mpi_libs = set(self._SITE_MPI_LIBS.keys())
        container_mpi_libs = set()
        output = self._get_command_output_in_container(["mount"])
        for line in output:
            if re.search(r".* on .*lib.*\.so(\.[0-9]+)* .*", line):
                container_lib = re.sub(r".* on .*(lib.*\.so(\.[0-9]+)*) .*", r"\1", line)
                container_lib_without_version_numbers = re.sub(r"(lib.*\.so)(\.[0-9]+)*", r"\1", container_lib)
                if container_lib_without_version_numbers in site_mpi_libs:
                    container_mpi_libs.add(container_lib)
        return container_mpi_libs

    def _get_mpi_dependency_libs_in_container(self):
        if self._is_mpi_support_in_container_enabled():
            return set(self._get_command_output_in_container(["ls", "/site-resources/mpi/lib"]))
        else:
            return set()

    def _get_mpi_bins_in_container(self):
        if self._is_mpi_support_in_container_enabled():
            return set(self._get_command_output_in_container(["ls", "/site-resources/mpi/bin"]))
        else:
            return set()

    def _get_mpi_config_files_in_container(self):
        config_files_in_container = set()
        for site_config_file in self._SITE_MPI_CONFIGURATION_FILES:
            if self._file_exists_in_container(site_config_file):
                config_files_in_container.add(site_config_file)
        return config_files_in_container

    def _is_mpi_support_in_container_enabled(self):
        return self._file_exists_in_container("/site-resources/mpi")

    def _file_exists_in_container(self, file_path):
        command = ["bash", "-c", "if [ -e " + file_path + " ]; then echo \"file exists\"; fi"]
        out = self._get_command_output_in_container(command)
        return out == ["file exists"]

    def _get_command_output_in_container(self, command):
        full_command = ["shifter", "--image="+self._container_image]
        if self._mpi_command_line_option:
            full_command.append("--mpi")
        full_command += command
        out = subprocess.check_output(full_command)
        return self._command_output_without_trailing_new_lines(out)

    def _assert_shifter_raises_mpi_error_containing_text(self, text):
        messages = self._get_shifter_mpi_support_error_messages()
        for message in messages:
            if text in message:
                return
        self.fail(  "Shifter didn't generate and MPI error containing the text \"{}\", " \
                    "but one was expected.".format(text))

    def _get_shifter_mpi_support_error_messages(self):
        full_command = ["shifter", "--image="+self._container_image, "--mpi", "echo"]
        try:
            subprocess.check_output(full_command, stderr=subprocess.STDOUT)
            self.fail("Shifter didn't generate any error, but at least one was expected.")
        except subprocess.CalledProcessError as ex:
            lines = self._command_output_without_trailing_new_lines(ex.output)
            return [line for line in lines if "[ MPI SUPPORT ] =ERROR=" in line]

    def _command_output_without_trailing_new_lines(self, out):
        lines = [line for line in out.split('\n')]
        # remove empty trailing elements due to trailing new lines
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
    unittest.main()
