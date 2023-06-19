const std = @import("std");
const glfw = @import("mach-glfw/build.zig");

inline fn thisDir() []const u8 {
    return comptime std.fs.path.dirname(@src().file) orelse ".";
}
// Although this function looks imperative, note that its job is to
// declaratively construct a build graph that will be executed by an external
// runner.
pub fn build(b: *std.Build) !void {
    const cflags = [_][]const u8{"-fstrict-aliasing"};

    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    const target = b.standardTargetOptions(.{});

    // Standard optimization options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall. Here we do not
    // set a preferred release mode, allowing the user to decide how to optimize.
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "wgpu-test",
        //.root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    exe.linkLibC();
    exe.linkLibCpp();
    try glfw.link(b, exe, .{});

    {//wgpu
        exe.addLibraryPath("include");
        exe.linkSystemLibrary("wgpu_native");

        if (target.isWindows())
        {
            //exe.addLibraryPath("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.35.32215/lib/x64");

            exe.linkSystemLibrary("ws2_32");
            exe.linkSystemLibrary("userenv");
            exe.linkSystemLibrary("bcrypt");
            exe.linkSystemLibrary("d3dcompiler_47");
            b.installFile("include/wgpu_native.dll", "bin/wgpu_native.dll");
        }
    }
    exe.addIncludePath("include");

    exe.addIncludePath("src");
    exe.addCSourceFile("src/framework.c", &cflags);
    exe.addCSourceFile("src/Program.c", &cflags);

    b.installFile("src/shader.wgsl", "bin/shader.wgsl");

    exe.want_lto = false;
    //exe.linkSystemLibrary("glfw3");

    // This declares intent for the executable to be installed into the
    // standard location when the user invokes the "install" step (the default
    // step when running `zig build`).
    b.installArtifact(exe);

    // This *creates* a Run step in the build graph, to be executed when another
    // step is evaluated that depends on it. The next line below will establish
    // such a dependency.
    const run_cmd = b.addRunArtifact(exe);

    // By making the run step depend on the install step, it will be run from the
    // installation directory rather than directly from within the cache directory.
    // This is not necessary, however, if the application depends on other installed
    // files, this ensures they will be present and in the expected location.
    run_cmd.step.dependOn(b.getInstallStep());

    // This allows the user to pass arguments to the application in the build
    // command itself, like this: `zig build run -- arg1 arg2 etc`
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    // This creates a build step. It will be visible in the `zig build --help` menu,
    // and can be selected like this: `zig build run`
    // This will evaluate the `run` step rather than the default, which is "install".
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
