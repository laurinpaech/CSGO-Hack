//
//  scanner.cpp
//
//  Created by Andre Kalisch on 23.09.16.
//  Copyright © 2016 Andre Kalisch. All rights reserved.
//

#include <sstream>

class Scanner
{
public:

    Byte *pRemote;
    const char *module;
    size_t moduleSize;
    uint64_t moduleStart;
    
    Scanner(uint64_t moduleStart, size_t moduleSize)
    {
        this->moduleStart = moduleStart;
        this->moduleSize = moduleSize;
        
        // pRemote points to buffer with Client.dylib Virtual Memory
        pRemote = new Byte[moduleSize];
        pRemote = mem->readData(moduleStart, moduleSize);
    }
    
    void unload()
    {
        delete[] pRemote;
    }
    
    bool MaskCheck(int nOffset, Byte* btPattern, const char * strMask, int sigLength)
    {
        for (int x = 0; x < sigLength; x++)
        {
            if (strMask[x] == '?')
            {
                continue;
            }

            if ((strMask[x] == 'x') && (btPattern[x] != pRemote[nOffset + x]))
            {
                return false;
            }
        }

        return true;
    }
    
    uint64_t scan(Byte* pSignature, const char *pMask, int sigLength)
    {
        if(!pRemote)
        {
            delete[] pRemote;
            return NULL;
        }
        
        // go through the module byte by byte and check the Mask
        for(int dwIndex = 0; dwIndex < moduleSize; dwIndex++)
        {
            if(MaskCheck(dwIndex, pSignature, pMask, sigLength))
            {
                return moduleStart + dwIndex;
            }
        }
        
        return NULL;
    }
    
    uint32_t getOffset(Byte* pSignature, const char * pMask, uint64_t start)
    {
        uint64_t signatureAddress = this->scan(pSignature, pMask, (int)strlen(pMask)) + start;
        uint32_t foundOffset = mem->read<uint32_t>(signatureAddress);
        
        printf("signatureAddress: 0x%llx, foundOffset: 0x%x\n", signatureAddress, foundOffset);
        
        return foundOffset;
    }
    
    uint32_t getPointer(Byte *pSignature, const char *pMask, uint32_t start)
    {
        // address of our signature
        uint64_t signatureAddress = this->scan(pSignature, pMask, (int)strlen(pMask)) + start;
        printf("signatureAddress: 0x%llx\n", signatureAddress);

        // Offset within client.dylib
        uint64_t fileOffset = signatureAddress - moduleStart;
        printf("fileOffset: 0x%llx\n", fileOffset);
        
        uint32_t foundOffset = mem->read<uint32_t>(signatureAddress) + (uint32_t)fileOffset;
        
        return foundOffset;
    }

};
