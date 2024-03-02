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

cc_binary(
    name = "epoll_minimum",
    srcs = ["epoll.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        ":thread_pool_200",
    ]
)

cc_binary(
    name = "test_hslongrun",
    srcs = ["test_hslongrun.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        "//libhsclient:hstcpcli",
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
        ":handler_socket",
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
        ":handler_socket",
])

cc_library(
    name = "thread_pool_200",
    hdrs = ["thread_pool_200.h"],
    srcs = ["thread_pool_200.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
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

cc_library(
    name = "handler_socket",
    hdrs = ["handler_socket.h"],
    srcs = ["handler_socket.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        "//libhsclient:hstcpcli",
        ":structs",
    ],
    copts = ["-Wno-deprecated-declarations"],
)

cc_library(
    name = "shared_lock",
    hdrs = ["shared_lock.h"],
    srcs = ["shared_lock.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
    ],
    copts = ["-Wno-deprecated-declarations"],
)
