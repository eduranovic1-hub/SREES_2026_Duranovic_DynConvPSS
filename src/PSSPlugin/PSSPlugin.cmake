set(PSSPLUGIN_NAME pss)				#Naziv prvog projekta u solution-u

file(GLOB PSSPLUGIN_CPP_COMMON_SOURCES  ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB PSSPLUGIN_CPP_COMMON_INCS  ${CMAKE_CURRENT_LIST_DIR}/src/*.h)
file(GLOB PSSPLUGIN_INC_GUI  ${NATID_SDK_INC}/gui/*.h)
file(GLOB PSSPLUGIN_INC_TD  ${NATID_SDK_INC}/td/*.h)
file(GLOB PSSPLUGIN_INC_CNT  ${NATID_SDK_INC}/cnt/*.h)
file(GLOB PSSPLUGIN_INC_MU  ${NATID_SDK_INC}/mu/*.h)
file(GLOB PSSPLUGIN_INC_MEM  ${NATID_SDK_INC}/mem/*.h)
file(GLOB PSSPLUGIN_INC_FO ${NATID_SDK_INC}/fo/*.h)
file(GLOB PSSPLUGIN_INC_SC ${NATID_SDK_INC}/sc/*.h)
file(GLOB PSSPLUGIN_INC_SYST ${NATID_SDK_INC}/syst/*.h)
file(GLOB PSSPLUGIN_INC_DENSE ${NATID_SDK_INC}/dense/*.h)
file(GLOB PSSPLUGIN_INC_SPARSE ${NATID_SDK_INC}/sparse/*.h)

# add shared library (plugin is a shared executatable binary file)
add_library(${PSSPLUGIN_NAME} SHARED ${PSSPLUGIN_CPP_COMMON_SOURCES} ${PSSPLUGIN_INC_GUI} ${PSSPLUGIN_CPP_COMMON_INCS} 
							${PSSPLUGIN_INC_TD} ${PSSPLUGIN_INC_SYST} 
							${PSSPLUGIN_INC_CNT} ${PSSPLUGIN_INC_MU} ${PSSPLUGIN_INC_MEM} ${PSSPLUGIN_INC_FO}
							${PSSPLUGIN_INC_SC} ${PSSPLUGIN_INC_DENSE} ${PSSPLUGIN_INC_SPARSE})

source_group("inc\\inc"        FILES ${PSSPLUGIN_CPP_COMMON_INCS})
source_group("inc\\gui"        FILES ${PSSPLUGIN_INC_GUI})
source_group("inc\\td"        FILES ${PSSPLUGIN_INC_TD})
source_group("inc\\cnt"        FILES ${PSSPLUGIN_INC_CNT})
source_group("inc\\dense"        FILES ${PSSPLUGIN_INC_DENSE})
source_group("inc\\mu"        FILES ${PSSPLUGIN_INC_MU})
source_group("inc\\mem"        FILES ${PSSPLUGIN_INC_MEM})
source_group("inc\\fo"        FILES ${PSSPLUGIN_INC_FO})
source_group("inc\\sc"        FILES ${PSSPLUGIN_INC_SC})
source_group("inc\\sparse"        FILES ${PSSPLUGIN_INC_SPARSE})
source_group("inc\\syst"        FILES ${PSSPLUGIN_INC_SYST})

source_group("src\\cpp"			FILES ${PSSPLUGIN_CPP_COMMON_SOURCES})

target_link_libraries(${PSSPLUGIN_NAME} debug ${MU_LIB_DEBUG} optimized ${MU_LIB_RELEASE} 
										debug ${MATRIX_LIB_DEBUG} optimized ${MATRIX_LIB_RELEASE}
									  debug ${NATGUI_LIB_DEBUG} optimized ${NATGUI_LIB_RELEASE})
									
target_compile_definitions(${PSSPLUGIN_NAME} PUBLIC PLUGIN_EXPORTS)

#setIDEPropertiesForLib(${PSSPLUGIN_NAME})



