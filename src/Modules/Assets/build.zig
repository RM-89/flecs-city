const std = @import("std");
const Vcpkg = @import("../../../zig/vcpkg.zig").Vcpkg;

pub fn build(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, vcpkg: Vcpkg) *std.Build.Step.Compile
{
    const mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    mod.addCSourceFiles(.{
        .files = &.{
            "src/Modules/Assets/Assets.cpp",
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

    return b.addLibrary(.{ .name = "Assets", .root_module = mod, .linkage = .dynamic });
}
