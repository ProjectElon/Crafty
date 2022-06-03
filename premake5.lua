workspace("Crafty")

	architecture("x64")
	systemversion("latest")
	startproject("Minecraft")

    configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	flags
	{
		"MultiProcessorCompile"
	}

    filter("system:windows")

		defines
		{
			"MC_PLATFORM_WINDOWS"
		}

	filter("system:linux")

		defines
		{
			"MC_PLATFORM_LINUX"
		}

	filter("system:macos")

		defines
		{
			"VN_PLATFORM_MACOS"
		}

    filter("configurations:Debug")
        defines("MC_DEBUG")
        runtime("Debug")
        symbols("on")

        defines
        {
            "MC_DEBUG"
        }

	filter("configurations:Release")
		defines("MC_RELEASE")
		runtime("Release")
		optimize("on")

		defines
		{
			"MC_RELEASE"
		}

    filter("configurations:Dist")
		defines("MC_DIST")
		runtime("Release")
		optimize("on")

		defines
		{
			"MC_DIST"
		}

project("Minecraft")
    location("src")
    kind("ConsoleApp")
    language("C++")
    cppdialect("C++17")
	staticruntime("on")

    files
	{
		"src/**.h",
		"src/**.cpp",
		"src/**.c"
	}

    includedirs
	{
		"src",
        "lib/include",
	}

	libdirs
	{
		"lib",
	}

    links
    {
        "opengl32.lib",
        "glfw3dll.lib",
        "OptickCore.lib"
    }

    defines
    {
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
    }

    targetdir("bin")
	objdir("obj")