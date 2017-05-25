/**
 *
 * @toggle key : CTRL + ALT (OPT) + V
 * Change modifier at line 226
 * Change keycode at line 227
 *
 */

#include <iostream>
#include <unistd.h>
#include <ApplicationServices/ApplicationServices.h>
#include <thread>
#include "memory/process.cpp"

pid_t   mainPid     = -1;
task_t  mainTask    = 0;

#include "memory/manager.cpp"
#include "memory/scanner.cpp"

bool ctr = false;
bool sft = false;
bool cmd = false;
bool alt = false;
bool states = true;

#include "utils/keyboard.h"

uint64_t glowInfoOffset;
uint64_t LocalPlayerBase;
uint64_t playerBase;
uint64_t CrosshairIDOffset;
uint64_t ActiveWeaponOffset;
uint64_t m_iGlowIndex = 0xAC10;

int iTeamNum;
int CrossHairID;

int healthOffset    = 0x134;
int teamOffset      = 0x128;

// For simulating a click for shooting
CGEventRef event;
CGPoint cursor;
CGEventRef click_down;
CGEventRef click_up;


bool statBool = true;

struct Color {
    float red;
    float green;
    float blue;
    float alpha;
};


void applyEntityGlow(mach_vm_address_t moduleStart, mach_vm_address_t glowStartAddress)
{
    while(states){
        for (int i = 0; i < 60; i++) {
            uint64_t memoryAddress  = mem->read<uint64_t>(moduleStart + playerBase + 0x20 * i);
            
            if (memoryAddress <= 0x0){
                continue;
            }
            
            int glowIndex           = mem->read<int>(memoryAddress + m_iGlowIndex);
            int health              = mem->read<int>(memoryAddress + 0x134);
            int playerTeamNum       = mem->read<int>(memoryAddress + 0x128);
            
            if (playerTeamNum == iTeamNum || playerTeamNum == 0) {
                continue;
            }
            
            if (health == 0) {
                health = 100;
            }
            
            Color color = {float((100 - health) / 100.0), float((health) / 100.0), 0.0f, 0.8f};
            
            uint64_t glowBase = glowStartAddress + (0x40 * glowIndex);
            
            mem->write<bool>(glowBase + 0x28, statBool);
            mem->write<Color>(glowBase + 0x8, color);
        }
        usleep(3000);
    }
}


// TODO: Create different shooting styles for different weapons
void triggerbot(mach_vm_address_t moduleStart, mach_vm_address_t playerAddress){
    
    CrossHairID = mem->read<int>(playerAddress + CrosshairIDOffset);
    
    // Dont shoot at nothing or weapons
    if (CrossHairID == 0 || CrossHairID > 60) {
        return;
    }
    
    uint64_t enemyAddress  = mem->read<uint64_t>(moduleStart + playerBase + 0x20 * (CrossHairID - 1));

    // Sanity check
    if (enemyAddress <= 0x0){
        return;
    }
    
    int playerTeam = mem->read<int>(enemyAddress + teamOffset);
    
    // Don't shoot at your own Team
    if (playerTeam == iTeamNum || playerTeam == 0) {
        return;
    }
    
    while(CrossHairID != 0 && playerTeam != iTeamNum && playerTeam != 0){
        
        
        CGEventPost(kCGHIDEventTap, click_down);
        usleep(100000);
        CGEventPost(kCGHIDEventTap, click_up);
        usleep(100000);
        
        CrossHairID = mem->read<int>(playerAddress + CrosshairIDOffset);
        
        if (CrossHairID == 0) {
            break;
        }
        
        enemyAddress    = mem->read<uint64_t>(moduleStart + playerBase + 0x20 * (CrossHairID - 1));
        playerTeam      = mem->read<int>(enemyAddress + 0x128);
    }
}



int main(int argc, const char * argv[])
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
        keyBoardListen();
    });
    
    mainPid     = g_cProc->getPID("csgo_osx64");
    mainTask    = g_cProc->getTask(mainPid);

    if (mainPid == -1) {
        printf("Cant find the PID of CSGO\n");
        return 0;
    } else {
        printf("Found CSGO PID: %i\n", mainPid);
    }
    
    off_t moduleLength = 0;
    mach_vm_address_t moduleStartAddress;
    g_cProc->getModule(mainTask, &moduleStartAddress, &moduleLength, "/client.dylib");

    if (mainTask) {
        printf("Found the Client.dylib address: 0x%llx \n", moduleStartAddress);
        printf("Module should end at 0x%llx\n", moduleStartAddress + moduleLength);
    } else {
        printf("Failed to get the Client.dylib address\n");
        return 0;
    }
    
    // Setup simulating a click
    event       = CGEventCreate(NULL);
    cursor      = CGEventGetLocation(event);
    
    click_down  = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, cursor, kCGMouseButtonLeft);
    click_up    = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, cursor, kCGMouseButtonLeft);
    
    
    Scanner * scanner = new Scanner(moduleStartAddress, moduleLength);
    
    LocalPlayerBase = scanner->getPointer(
        (Byte *) "\x89\xD6\x41\x89\x00\x49\x89\x00\x48\x8B\x1D\x00\x00\x00\x00\x48\x85\xDB\x74\x00",
        "xxxx?xx?xxx????xxxx?",
        0xB
    ) + 0x4;
    
    printf("playerAddress: 0x%llx Localplayer: 0x%llx\n", moduleStartAddress + LocalPlayerBase, LocalPlayerBase);
    
    playerBase = scanner->getPointer(
        (Byte *) "\x48\x8D\x1D\x00\x00\x00\x00\x48\x89\xDF\xE8\x00\x00\x00\x00\x48\x8D\x3D\x00\x00\x00\x00\x48\x8D\x15\x00\x00\x00\x00\x48\x89\xDE",
        "xxx????xxxx????xxx????xxx????xxx",
        0x3
    ) + 0x2C;
    
    printf("playerBase Global: 0x%llx playerBase: 0x%llx\n", moduleStartAddress + playerBase, playerBase);
    
    glowInfoOffset = scanner->getPointer(
        (Byte *) "\x48\x8D\x3D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x85\xC0\x0F\x84\x00\x00\x00\x00\x48\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\x48\x8D\x35\x00\x00\x00\x00",
        "xxx????x????xxxx????xxx????xxxxxxx????",
        0x22
    ) + 0x4;
    
    uint64_t glowObjectLoopStartAddress = mem->read<uint64_t>(moduleStartAddress + glowInfoOffset);
    
    printf("glowInfoOffset Global: 0x%llx glowInfoOffset: 0x%llx\n", moduleStartAddress + glowInfoOffset, glowInfoOffset);
    
    CrosshairIDOffset = scanner->getOffset(
        (Byte *) "\x76\x35\x8B\x83\x00\x00\x00\x00\x85\xC0\x75\x2D\x83\xBB\x00\x00\x00\x00\x00\x74\x2B\x48\x8D\xBB",
        "xxxx????xxxxxx????xxxxxx",
         0x4
    );
    
    ActiveWeaponOffset = scanner->getOffset(
        (Byte *) "\x76\x35\x8B\x83\x00\x00\x00\x00\x85\xC0\x75\x2D\x83\xBB\x00\x00\x00\x00\x00\x74\x2B\x48\x8D\xBB",
        "xxxx????xxxxxx????xxxxxx",
        0x4
    );
        
    // run hack until CS:GO is running
    while (mainPid != -1 && mainTask != 0)
    {
        if (states)
        {
            // Get address of the LocalPlayerStruct (LocalPlayerBase is a pointer to the LocalPlayerStruct)
            uint64_t playerAddress = mem->read<uint64_t>(moduleStartAddress + LocalPlayerBase);
            iTeamNum = mem->read<int>(playerAddress + teamOffset);
            
            applyEntityGlow(moduleStartAddress, glowObjectLoopStartAddress);
            triggerbot(moduleStartAddress, playerAddress);
        }
        
        usleep(3000);
    }
    
    // Cleanup
    std::system("clear");
    printf("CS:GO closed. Please restart CS:GO and the hack.");
    CFRelease(click_down);
    CFRelease(click_up);
    CFRelease(event);
    scanner->unload();
    delete scanner;
    return 0;
}
