const std = @import("std");

pub const Windows = struct
{
    /// Links Raylib and Windows dependencies with some features disabled.
    /// Mainly for compatibility with ENet.
    pub fn linkRaylib(mod: *std.Build.Module) void
    {
        mod.linkSystemLibrary("winmm", .{});
        mod.linkSystemLibrary("ws2_32", .{});
        mod.addCMacro("WIN32_LEAN_AND_MEAN", "1");
        mod.addCMacro("NOGDI", "1");
        mod.addCMacro("NOUSER", "1");

        mod.linkSystemLibrary("raylib.dll", .{});
    }
};

pub const Tests = struct
{
    const source_extension = ".cpp";

    /// Collects every test source from `dir`.
    pub fn collectFrom(b: *std.Build, dir: []const u8) ![][]const u8
    {
        const io = b.graph.io;
        var sources: std.ArrayList([]const u8) = .empty;

        var handle = b.build_root.handle.openDir(io, dir, .{ .iterate = true }) catch |err| switch (err)
        {
            error.FileNotFound => return finish(b, &sources),
            else => return err,
        };
        defer handle.close(io);

        try collectInto(b, dir, handle, &sources);

        return finish(b, &sources);
    }

    /// Collects every test source from `<base>/<module>/Tests`.
    pub fn collectFromModules(b: *std.Build, base: []const u8) ![][]const u8
    {
        const io = b.graph.io;
        var sources: std.ArrayList([]const u8) = .empty;

        var modules = b.build_root.handle.openDir(io, base, .{ .iterate = true }) catch |err| switch (err)
        {
            error.FileNotFound => return finish(b, &sources),
            else => return err,
        };
        defer modules.close(io);

        var module_it = modules.iterate();
        while (try module_it.next(io)) |module_entry|
        {
            if (module_entry.kind != .directory) continue;

            var module = try modules.openDir(io, module_entry.name, .{ .iterate = true });
            defer module.close(io);

            var tests = module.openDir(io, "Tests", .{ .iterate = true }) catch continue;

            const prefix = b.fmt("{s}/{s}/Tests", .{ base, module_entry.name });
            try collectInto(b, prefix, tests, &sources);

            tests.close(io);
        }

        return finish(b, &sources);
    }

    /// Appends the test sources found in an opened directory to `sources`.
    fn collectInto(b: *std.Build, prefix: []const u8, dir: std.Io.Dir, sources: *std.ArrayList([]const u8)) !void
    {
        const io = b.graph.io;

        var it = dir.iterate();
        while (try it.next(io)) |entry|
        {
            if (entry.kind != .file) continue;
            if (!std.mem.endsWith(u8, entry.name, source_extension)) continue;

            try sources.append(b.allocator, b.fmt("{s}/{s}", .{ prefix, entry.name }));
        }
    }

    /// Sorts paths to compensate for nondeterministic discovery order
    fn finish(b: *std.Build, sources: *std.ArrayList([]const u8)) ![][]const u8
    {
        std.mem.sort([]const u8, sources.items, {}, struct
        {
            fn lessThan(_: void, a: []const u8, c: []const u8) bool
            {
                return std.mem.lessThan(u8, a, c);
            }
        }.lessThan);

        const collected = try sources.toOwnedSlice(b.allocator);

        if (b.verbose)
        {
            for (collected) |source|
            {
                std.debug.print("Discovered test source: {s}\n", .{source});
            }
        }

        return collected;
    }
};
