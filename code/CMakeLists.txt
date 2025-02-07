

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/base BASE_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp JSON_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/mysql MYSQL_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/redis REDIS_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/api API_LIST)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)


#INCLUDE_DIRECTORIES(/usr/include/fastdfs /usr/include/fastcommon /usr/local/include/hiredis /usr/include/mysql)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rapidjson/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/base)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mysql)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/redis)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/api)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/muduo)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SERVER_CODE 
    main.cc 
    http_parser.cc 
    http_parser_wrapper.cc 
    http_conn.cc
    ${BASE_LIST}
    ${API_LIST} 
    ${MYSQL_LIST} 
    ${REDIS_LIST} 
    ${JSON_LIST}
)

# add_subdirectory(rapidjson)
add_subdirectory(spdlog)
add_subdirectory(muduo)
add_executable(Cloud-disk ${SERVER_CODE})
target_link_libraries(Cloud-disk pthread mysqlclient hiredis muduo_net spdlog)