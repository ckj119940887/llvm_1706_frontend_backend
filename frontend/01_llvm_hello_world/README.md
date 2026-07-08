# install
```
尽量不要直接使用apt-get install安装clang这些工具链，最好通过llvm-project来编译
产生对应的工具链
```

# ubuntu 22.04 (llvm 17.06, https://github.com/llvm/llvm-project/releases/tag/llvmorg-17.0.6/llvm-project-17.0.6.src.tar.xz)
```
sudo apt -y install libncurses5 gdb python3
sudo apt -y install gcc g++ git cmake ninja-build zlib1g-dev vim curl

LLVM_USE_LINKER: 使用gold,比lld快很多，同时占用资源较少
CMAKE_BUILD_TYPE: 使用RelWithDebInfo, 不像Debug那么多符号信息，也不想Release那样没有任何符号信息
LLVM_TARGETS_TO_BUILD: X86;RISCV
BUILD_SHARED_LIBS: ON 使用动态库编(默认是静态库，很慢)
LLVM_OPTIMIZED_TABLEGEN: ON (单独构建优化版本的table-gen,llvm的指令集描述文件依赖table-gen的解析，理论上table-gen速度快，构建速度就会加快)
LLVM_ENABLE_PROJECTS: 使用clang (同时构建一下clang,便于生成ir,调试对比)
CMAKE_INSTALL_PREFIX: 构建完成之后，llvm的库安装所在的目录
```

```
进入build目录执行如下命令

cmake ../llvm-project/llvm -GNinja -DLLVM_OPTIMIZED_TABLEGEN=ON -DLLVM_TARGETS_TO_BUILD=X86 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=ON -DLLVM_USE_LINKER=gold -DLLVM_ENABLE_PROJECTS=clang  -DCMAKE_INSTALL_PREFIX=../llvm_install_dir

ninja -j8

ninja install

这里有个问题，就是TYPE的问题，如果使用debug，会非常慢，同时会占用非常多的空间，而使用release则会非常快.
```

```
此外，-DCMAKE_INSTALL_PREFIX，该选项如果不指定，二进制程序默认是装在/usr/local/bin，
而头文件默认是装在/usr/local/include。如果指定了安装目录，如果想方便nvim的
头文件包含，可以建立软链接，将指定目录的头文件指向/usr/include下,使用如下命令：
sudo ln -s /home/ckj/git_dir/tools/llvm-tools/include /usr/include
```

# .vscode
## launch.json
```
Run -> Add Configuration -> CMake Debuger
```

```
{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "llvm_hello_world debug",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/bin/llvm_hello_world",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "preLaunchTask": "build",
            "environment": [],
            "externalConsole": false,
            "osx": {
                "MIMode": "lldb"
            },
            "linux": {
                "MIMode": "gdb"
            }
        }
    ]
}
```

## settings.json
```
Ctrl + Shift + P -> Preferences: Open Workspace Settings (json)
```

```
{
    "cmake.configureOnOpen": true,
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "files.associations": {
        "*.tcc": "cpp",
        "array": "cpp",
        "atomic": "cpp",
        "bit": "cpp",
        "bitset": "cpp",
        "cctype": "cpp",
        "charconv": "cpp",
        "chrono": "cpp",
        "cinttypes": "cpp",
        "clocale": "cpp",
        "cmath": "cpp",
        "codecvt": "cpp",
        "compare": "cpp",
        "concepts": "cpp",
        "condition_variable": "cpp",
        "cstdarg": "cpp",
        "cstddef": "cpp",
        "cstdint": "cpp",
        "cstdio": "cpp",
        "cstdlib": "cpp",
        "cstring": "cpp",
        "ctime": "cpp",
        "cwchar": "cpp",
        "cwctype": "cpp",
        "deque": "cpp",
        "list": "cpp",
        "map": "cpp",
        "set": "cpp",
        "string": "cpp",
        "unordered_map": "cpp",
        "vector": "cpp",
        "exception": "cpp",
        "algorithm": "cpp",
        "functional": "cpp",
        "iterator": "cpp",
        "memory": "cpp",
        "memory_resource": "cpp",
        "numeric": "cpp",
        "optional": "cpp",
        "random": "cpp",
        "ratio": "cpp",
        "regex": "cpp",
        "string_view": "cpp",
        "system_error": "cpp",
        "tuple": "cpp",
        "type_traits": "cpp",
        "utility": "cpp",
        "fstream": "cpp",
        "initializer_list": "cpp",
        "iomanip": "cpp",
        "iosfwd": "cpp",
        "iostream": "cpp",
        "istream": "cpp",
        "limits": "cpp",
        "new": "cpp",
        "ostream": "cpp",
        "sstream": "cpp",
        "stdexcept": "cpp",
        "streambuf": "cpp",
        "thread": "cpp",
        "cfenv": "cpp",
        "cinttypes": "cpp",
        "typeinfo": "cpp",
        "variant": "cpp",
        "mutex": "cpp",
        "shared_mutex": "cpp",
        "any": "cpp",
        "future": "cpp"
    }
}
```

## tasks.json
```
Terminal -> Configure Default Build Task
选择 C/C++: gcc build active file
会自动生成tasks.json，我们使用默认的配置就可以，如果你的文件是cpp结尾，将会自动使用g++。
```

```
{
	"version": "2.0.0",
	"tasks": [
		{
			"label": "build_dir",
			"command": "mkdir",
			"type": "shell",
			"args": [
				"-p",
				"build"
			]
		},
		{
			"label": "cmake",
			"type": "shell",
			"command": "cmake",
			"args": [
				"../",
				"-GNinja",
				"-DLLVM_DIR=/home/git_dir/llvm_project_1706/llvm_install_dir/lib/cmake/llvm",
				"-DCMAKE_BUILD_TYPE=Debug",
				"-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
			],
			"options": {
				"cwd": "${workspaceFolder}/build"
			},
			"dependsOn": [
				"build_dir"
			]
		},
		{
			"label": "build",
			"group": "build",
			"type": "shell",
			"command": "ninja",
			"args": [
				"-j4"
			],
			"options": {
				"cwd": "${workspaceFolder}/build"
			},
			"problemMatcher": "$gcc",
			"dependsOn": [
				"cmake"
			]
		}
	]
}
```

## c_cpp_properties.json
```
Ctrl + Shift + P 然后输入 C/C++: Edit Configurations (JSON)，然后在includePath中添加其他的目录。
```

```
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/../llvm_project_1706/build/include"
            ],
            "defines": [],
            "compilerPath": "/usr/bin/g++",
            "cStandard": "c17",
            "cppStandard": "c++20",
            "mergeConfigurations": false,
            "browse": {
                "path": [
                    "${workspaceFolder}/**"
                ],
                "limitSymbolsToIncludedHeaders": true
            },
            "configurationProvider": "ms-vscode.cmake-tools",
            "compileCommands": "${config:cmake.buildDirectory}/compile_commands.json"
        }
    ],
    "version": 4
}
```