const std = @import("std");
const Vcpkg = @import("../../../zig/vcpkg.zig").Vcpkg;
const build_utils = @import("../../../zig/utils.zig");

pub fn build(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, vcpkg: Vcpkg, assets: *std.Build.Step.Compile) *std.Build.Step.Compile
{
    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    mod.addCSourceFiles(.{
        .files = &.{
            "src/Modules/Core/Core.cpp",
            "src/Modules/Core/ECS/Phases.cpp",
        },
        .flags = &.{"-std=c++17", "-fvisibility=hidden", "-fvisibility-inlines-hidden"},
    });

    mod.addIncludePath(b.path("src/Public"));
    mod.addIncludePath(b.path("src/Modules"));
    mod.addIncludePath(vcpkg.inc_path);
    mod.addLibraryPath(vcpkg.lib_path);

    mod.addCMacro("EXPORTS", "");
    mod.addCMacro("SPDLOG_HEADER_ONLY", "1");
    mod.addCMacro("FMT_HEADER_ONLY", "1");

    mod.linkSystemLibrary("c++", .{});
    mod.linkLibrary(assets);

    if (target.result.os.tag == .windows)
    {
        build_utils.Windows.linkRaylib(mod);
        mod.linkSystemLibrary("flecs.dll", .{});
    }
    else
    {
        mod.linkSystemLibrary("flecs", .{});
        mod.linkSystemLibrary("raylib", .{});
    }

    return b.addLibrary(.{ .name = "Core", .root_module = mod, .linkage = .dynamic });
}
