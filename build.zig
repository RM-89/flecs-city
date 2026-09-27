const std = @import("std");
const zcc = @import("compile_commands");
const Vcpkg = @import("zig/vcpkg.zig").Vcpkg;
const utils = @import("zig/utils.zig");
const core = @import("src/Modules/Core/build.zig");

pub fn build(b: *std.Build) !void
{
    var targets: std.ArrayList(*std.Build.Step.Compile) = .empty;

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const vcpkg = Vcpkg.init(b, target);

    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    mod.addCSourceFiles(.{
        .files = &.{
            "src/main.cpp",
            "src/Private/Application/Application.cpp",
            "src/Private/Environment/Options.cpp",
            "src/Private/Network/ClientThread.cpp",
            "src/Private/Network/NetworkThread.cpp",
            "src/Private/Network/ServerThread.cpp",
        },
        .flags = &.{"-std=c++17"},
    });

    mod.addCMacro("SPDLOG_HEADER_ONLY", "1");
    mod.addCMacro("FMT_HEADER_ONLY", "1");

    mod.addIncludePath(b.path("src/Public"));
    mod.addIncludePath(b.path("src/Private"));
    mod.addIncludePath(b.path("src/Modules"));
    mod.addIncludePath(vcpkg.inc_path);
    mod.addLibraryPath(vcpkg.lib_path);

    mod.linkSystemLibrary("c++", .{});
    mod.linkSystemLibrary("enet", .{});

    if (target.result.os.tag == .windows)
    {
        utils.Windows.linkRaylib(mod);
        mod.linkSystemLibrary("flecs.dll", .{});
        mod.linkSystemLibrary("glfw3dll", .{});

        const vcpkg_binaries = b.addInstallDirectory(.{
            .source_dir = vcpkg.bin_path,
            .install_dir = .bin,
            .install_subdir = "",
        });

        b.getInstallStep().dependOn(&vcpkg_binaries.step);
    }
    else
    {
        mod.linkSystemLibrary("raylib", .{});
        mod.linkSystemLibrary("flecs", .{});
        mod.linkSystemLibrary("glfw", .{});
    }

    const core_lib = core.build(b, target, optimize, vcpkg);
    mod.linkLibrary(core_lib);

    const exe = b.addExecutable(.{
        .name = "flecs_city",
        .root_module = mod,
    });

    b.installArtifact(exe);
    b.installArtifact(core_lib);

    try targets.append(b.allocator, exe);
    try targets.append(b.allocator, core_lib);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    // Tests
    
    var test_sources: std.ArrayList([]const u8) = .empty;
    try test_sources.append(b.allocator, "src/Tests/main.cpp");
    try test_sources.appendSlice(b.allocator, try utils.Tests.collectFrom(b, "src/Tests/Unit"));
    try test_sources.appendSlice(b.allocator, try utils.Tests.collectFromModules(b, "src/Modules"));

    const gtest_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    gtest_mod.addCSourceFiles(.{
        .files = &.{ b.fmt("{s}/{s}/src/gtest-all.cc", .{ vcpkg.root, vcpkg.triplet }) },
        .flags = &.{"-std=c++17", "-Wno-implicit-int-conversion"},
    });

    gtest_mod.addIncludePath(vcpkg.inc_path);
    gtest_mod.addIncludePath(b.path(b.fmt("{s}/{s}", .{ vcpkg.root, vcpkg.triplet })));
    gtest_mod.linkSystemLibrary("c++", .{});

    const gtest_lib = b.addLibrary(.{
        .name = "gtest",
        .root_module = gtest_mod,
        .linkage = .static
    });

    const test_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    test_mod.addCSourceFiles(.{
        .files = try test_sources.toOwnedSlice(b.allocator),
        .flags = &.{"-std=c++17"},
    });

    test_mod.addCMacro("SPDLOG_HEADER_ONLY", "1");
    test_mod.addCMacro("FMT_HEADER_ONLY", "1");

    test_mod.addIncludePath(b.path("src/Public"));
    test_mod.addIncludePath(vcpkg.inc_path);
    test_mod.addLibraryPath(vcpkg.lib_path);

    test_mod.linkSystemLibrary("c++", .{});
    test_mod.linkLibrary(gtest_lib);
    test_mod.linkLibrary(core_lib);

    const test_exe = b.addExecutable(.{
        .name = "tests",
        .root_module = test_mod,
    });

    b.installArtifact(test_exe);

    try targets.append(b.allocator, test_exe);

    const test_cmd = b.addRunArtifact(test_exe);
    test_cmd.step.dependOn(b.getInstallStep());

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&test_cmd.step);

    if (b.args) |args|
    {
        run_cmd.addArgs(args);
        test_cmd.addArgs(args);
    }

    const cdb = zcc.createStep(b, .{
        .name = "cdb",
        .targets = targets.items,
    });

    b.getInstallStep().dependOn(cdb);
}
