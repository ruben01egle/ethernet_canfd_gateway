# Dual-Core Debugging Guide (STM32H7 CM7 + CM4) via ST-LINK

This repository is pre-configured for simultaneous dual-core debugging using Visual Studio Code, the **Cortex-Debug** extension, and the native **ST-LINK GDB Server**. Because both cores share a single physical SWD connection, a specific Master-Slave chaining sequence must be followed.

---

## 1. Prerequisites & Required VS Code Settings

To allow VS Code to correctly resolve the project layout, cross-compiler paths, and the ST-LINK executable, ensure that your workspace settings (`.vscode/settings.json`) contain the path to the stlink debugger:

```json
{
    "cortex-debug.stlinkPath": "/home/ruben/.local/share/stm32cube/bundles/stlink-gdbserver/7.13.0+st.3/bin/ST-LINK_gdbserver",
}
```
> Important: Make sure the cortex-debug.stlinkPath accurately points to your local ST-LINK_gdbserver binary

## 2. Debugging Workflow & Architecture

The debugging session uses a coordinated Launch + Attach mechanism managed via .vscode/launch.json:

#### 1. Debug CM7+CM4 (ST-Link) (Master Launch):
* Triggers the Build all task to recompile both cores via the top-level Makefile.
* Connects to the Cortex-M7 (Core 0), flashes the binary files for both cores (loadFiles), resets the micro-controller, and halts execution at main().

#### 2. Attach CM4 (ST-Link) (Slave Attach):
* Automatically spawned after a 5-second delay (chainedConfigurations).
* Connects to the already running Cortex-M4 (Core 3) on the fly without stopping the M7 core or altering flash contents.

## 3. Critical Timing & Core Synchronization Note

On the STM32H7 dual-core architecture, the Cortex-M4 core may boot and run code faster than the debugger can establish its secondary GDB connection.

If the Cortex-M4 executes past your intended initialization breakpoints before the Attach command completes (during the 5-second chain delay), you will miss the initial execution sequence.