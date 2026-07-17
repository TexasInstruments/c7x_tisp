

    .global TISP_TEST_c7xSetL2CC
TISP_TEST_c7xSetL2CC:
    MVC .S1 A4,     ECR385      ;; L2 Cache Control Register
    RET .B1

    .global TISP_TEST_c7xGetL2CC
TISP_TEST_c7xGetL2CC:
    MVC .S1 ECR385, A4  ;; L2 Cache Control Register
    RET .B1
