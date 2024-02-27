cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":request_handler",
        ":maria_database",
        ":from_http",
        ":thread_pool",
        ":io_thread_pool",
        ":structs",
    ]
)

cc_library(
    name = "structs",
    srcs = ["structs.h"],
    deps = [
    ]
)

cc_library(
    name = "to_json",
    hdrs = ["to_json.h"],
    srcs = ["to_json.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":structs",
    ]
)

cc_library(
    name = "from_http",
    hdrs = ["from_http.h"],
    srcs = ["from_http.cc"],
    deps = [
        "@abseil//:abseil",
        "@abseil//:abseil-absl_time",
        "@glog//:glog",
        ":structs",
    ]
)

cc_library(
    name = "request_handler",
    hdrs = ["request_handler.h"],
    srcs = ["request_handler.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":structs",
        ":to_json",
        ":from_http",
        ":maria_database",
        ":time",
    ]
)


cc_library(
    name = "time",
    hdrs = ["time.h"],
    srcs = ["time.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
    ]
)

cc_library(
    name = "maria_database",
    hdrs = ["maria_database.h"],
    srcs = ["maria_database.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        "@mariadb-connector-c//:mariadb-connector-c",
        ":structs",
        ":to_json",
    ]
)

cc_library(
    name = "thread_pool",
    hdrs = ["thread_pool.h"],
    srcs = ["thread_pool.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":request_processor",
        ":maria_database",
])


cc_library(
    name = "io_thread_pool",
    hdrs = ["io_thread_pool.h"],
    srcs = ["io_thread_pool.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":thread_pool",
    ])

cc_library(
    name = "request_processor",
    hdrs = ["request_processor.h"],
    srcs = ["request_processor.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":request_handler",
        ":from_http",
        ":structs"
    ]
)
