cc_library(
    name = "allocator",
    srcs = ["allocator.hpp"],
)

cc_library(
    name = "auto_addrinfo",
    srcs = ["auto_addrinfo.hpp"],
    deps = [
        # ":my_global",
        ":util",
    ]
)

cc_library(
    name = "auto_file",
    srcs = ["auto_file.hpp"],
)

cc_library(
    name = "auto_ptrcontainer",
    srcs = ["auto_ptrcontainer.hpp"],
)

cc_library(
    name = "config",
    hdrs = ["config.hpp"],
    srcs = ["config.cpp"],
)

cc_library(
    name = "escape",
    hdrs = ["escape.hpp"],
    srcs = ["escape.cpp"],
    deps = [
        ":fatal",
        ":string_buffer",
        ":string_util",
    ]
)

cc_library(
    name = "fatal",
    hdrs = ["fatal.hpp"],
    srcs = ["fatal.cpp"],
)

cc_library(
    name = "hslongrun",
    srcs = ["hslongrun.cpp"],
    deps = [
        "@mariadb-connector-c//:mariadb-connector-c",
        ":auto_ptrcontainer",
        ":hstcpcli",
        ":mutex",
        ":socket",
        ":string_util",
        ":util",
    ],
    copts = ["-fpermissive", "-w"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "hstcpcli",
    hdrs = ["hstcpcli.hpp"],
    srcs = ["hstcpcli.cpp"],
    deps = [
        ":auto_addrinfo",
        ":auto_file",
        ":config",
        ":escape",
        ":socket",
        ":string_util",
        ":string_buffer",
        ":string_ref",
        ":util",
    ],
    visibility = ["//visibility:public"],
    copts = ["-fpermissive", "-w"],
)

cc_library(
    name = "mutex",
    srcs = ["mutex.hpp"],
    deps = [
        ":fatal",
        ":util",
    ]
)

cc_library(
    name = "socket",
    hdrs = ["socket.hpp"],
    srcs = ["socket.cpp"],
    deps = [
        ":auto_addrinfo",
        ":auto_file",
        ":config",
        ":fatal",
        ":string_util",
    ],
    copts = ["-fpermissive", "-w"],
)

cc_library(
    name = "string_buffer",
    srcs = ["string_buffer.hpp"],
    deps = [
        ":util",
        ":allocator",
        ":fatal",
    ]
)

cc_library(
    name = "string_ref",
    srcs = ["string_ref.hpp"],
)

cc_library(
    name = "string_util",
    hdrs = ["string_util.hpp"],
    srcs = ["string_util.cpp"],
    deps = [
        ":string_buffer",
        ":string_ref",
    ]
)

cc_library(
    name = "thread",
    srcs = ["thread.hpp"],
    deps = [
        ":fatal",
    ]
)

cc_library(
    name = "util",
    srcs = ["util.hpp"],
)

cc_library(
    name = "my_global",
    srcs = ["my_global.h"],
    copts = ["-fpermissive"],
)

cc_library(
    name = "handlersocket",
    hdrs = ["handlersocket.h"],
    srcs = ["handlersocket.cpp"],
    copts = ["-fpermissive", "-w"],
    deps =[":hstcpcli"]
)
