import numpy as np
from sys import platform
from subprocess import call

format_functions = {
    "float": lambda x: f"{x:.6f}",
    "int": lambda x: f"{x}",
    "int32_t": lambda x: f"{x}",
    "short": lambda x: f"{x}",
    "int32_t": lambda x: f"{x}",
}


class TensorToCppHeader:
    def __init__(self, header_file_name, mode="new"):
        """
        Initializes the TensorToCppHeader class.

        Args:
            header_file_name (str): Name of the C++ header file.
            mode (str): Mode to open the file. Can be 'new' or 'append'. Defaults to 'new'.
        """
        self.header_file_name = header_file_name
        if mode == "new":
            with open(self.header_file_name, "w") as f:
                f.write("#pragma once\n\n")
                f.close()
        elif mode == "append":
            pass  # Do nothing, as we don't want to overwrite the existing file
        else:
            raise ValueError("Invalid mode. Mode can be 'new' or 'append'.")

    def write_tensor_to_header(self, tensor, array_name, dtype):
        """
        Writes the tensor to the header file.

        Args:
            tensor (np.ndarray): Tensor to write.
            array_name (str): Name of the static array in the header file.
        """
        with open(self.header_file_name, "a") as f:
            # Determine the shape of the tensor
            shape = tensor.shape

            attributes = '__attribute__((section(".staticData"))) '
            # Print array declaration
            f.write(attributes)
            f.write(f"const {dtype} {array_name}[{self.get_total_size(shape)}] = {{\n")

            # Print tensor values
            self.print_tensor_values(f, tensor, shape, dtype)

            f.write(f"}};\n\n")
            f.close()

            # self.indent()

    def get_total_size(self, shape):
        """
        Calculates the total size of the tensor.

        Args:
            shape (tuple): Shape of the tensor.

        Returns:
            int: Total size of the tensor.
        """
        total_size = 1
        for dim in shape:
            total_size *= dim
        return total_size

    def print_tensor_values(self, f, tensor, shape, dtype):
        """
        Prints the tensor values to the file.

        Args:
            f (file): File object.
            tensor (np.ndarray): Tensor to print.
            shape (tuple): Shape of the tensor.
        """
        format_func = format_functions.get(dtype)

        if len(shape) == 1:
            # Print 1D array values
            for i, value in enumerate(tensor):
                f.write(f"{format_func(value)}")
                if i < len(tensor) - 1:
                    f.write(f",")
                else:
                    f.write(f",\n")
        else:
            # Print multi-dimensional array values
            for i, sub_tensor in enumerate(tensor):
                if i > 0:
                    f.write(f"\n")
                self.print_tensor_values(f, sub_tensor, shape[1:], dtype)
        # f.close()

    def indent(self):
        if platform == "linux" or platform == "linux2":
            call(
                [
                    "indent",
                    "-nut",
                    "-i3",
                    # "-c55",
                    "-l100",
                    self.header_file_name,
                ]
            )
            call(["rm", self.header_file_name + "~"])  # remove backup file


# # Example usage:
# header_file_name = 'tensors.h'

# tensor_to_cpp_header = TensorToCppHeader(header_file_name)

# tensor1 = np.array([1.0, 2.0, 3.0])
# tensor_to_cpp_header.write_tensor_to_header(tensor1, 'tensor1')

# tensor2 = np.array([[4.0, 5.0], [6.0, 7.0]])
# tensor_to_cpp_header.write_tensor_to_header(tensor2, 'tensor2')
