# Toolchain file for the S32K344 (Arm Cortex-M7) with GNU Arm Embedded GCC.
#   cmake --preset arm-libs
#
# FPU: -mfpu=fpv5-sp-d16 is deliberate. It emits single-precision hardware float only, so it is safe
# whether the part's FPU is single or double precision. All VCU code uses float (the CAN generator is
# run with use_float). CONFIRM the FPU of your exact part number in the S32K3 reference manual before
# changing this. Linker script, startup code and the NXP RTD are NOT part of this scaffold.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_RANLIB arm-none-eabi-ranlib)

# No linking while probing the compiler: there is no linker script yet.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(VCU_MCU_FLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")
set(CMAKE_C_FLAGS_INIT "${VCU_MCU_FLAGS} -ffunction-sections -fdata-sections -fno-common")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
