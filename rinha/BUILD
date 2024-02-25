cc_binary(
    name = "main",
    srcs = ["main.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
        "@threadpool//:threadpool",
        "@simdjson//:simdjson",
        ":request_handler",
        ":maria_database",
        ":shared_lock",
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
        "@simdjson//:simdjson",
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
    name = "shared_lock",
    hdrs = ["shared_lock.h"],
    srcs = ["shared_lock.cc"],
    deps = [
        "@abseil//:abseil",
        "@glog//:glog",
    ]
)

cc_library(
    name = "shared_count",
    hdrs = ["shared_count.h"],
    srcs = ["shared_count.cc"],
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
        ":shared_lock",
        ":shared_count",
    ]
)
