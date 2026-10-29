get_filename_component(E2E_VECTORS "${CMAKE_CURRENT_SOURCE_DIR}/../../can/tests/vectors/e2e_vectors.csv" ABSOLUTE)

function(vcu_add_test name)
  add_executable(${name} ${name}.c)
  target_link_libraries(${name} PRIVATE ${ARGN})
  target_compile_options(${name} PRIVATE ${VCU_WARNING_FLAGS})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

vcu_add_test(test_crc can_e2e)
target_compile_definitions(test_crc PRIVATE E2E_VECTORS_CSV="${E2E_VECTORS}")

vcu_add_test(test_e2e can_e2e)
vcu_add_test(test_dti can_dti)

if(NOT TARGET can_gen)
  message(FATAL_ERROR "can/gen has no .c files. Run: python can/tools/generate.py")
endif()
vcu_add_test(test_gen can_e2e can_gen)
