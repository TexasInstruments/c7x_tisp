import numpy as np


class MatrixMultiplication:
    def __init__(self):
        """
        Initializes the MatrixMultiplication class.

        Args:
            None

        Returns:
            None
        """

    def execute(self, A, B):
        """
        Multiplies two matrices.

        Args:
            A (numpy.ndarray): The first matrix.
            B (numpy.ndarray): The second matrix.

        Returns:
            The result of the matrix multiplication.
        """
        return A @ B
