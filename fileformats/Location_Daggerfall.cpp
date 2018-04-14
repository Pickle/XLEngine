#include "Location_Daggerfall.h"
#include "../EngineSettings.h"
#include "../fileformats/ArchiveTypes.h"
#include "../fileformats/ArchiveManager.h"
#include "../memory/ScratchPad.h"
#include <cstdio>
#include <cstdlib>
#include <memory.h>
#include <cstring>
#include <cassert>


namespace
{

#pragma pack(push)
#pragma pack(1)

struct LocationRec
{
    int32_t oneValue;
    int16_t NullValue1;
    int8_t  NullValue2;
    int32_t XPosition;
    int32_t NullValue3;
    int32_t YPosition;
    int32_t LocType;
    int32_t Unknown2;
    int32_t Unknown3;
    int16_t oneValue2;
    uint16_t LocationID;
    int32_t NullValue4;
    int16_t Unknown4;
    int32_t Unknown5;
    char NullValue[26];
    char LocationName[32];
    char Unknowns[9];
    int16_t PostRecCount;
};

#pragma pack(pop)

const char _aszRMBHead[][5]=
{
    "TVRN",
    "GENR",
    "RESI",
    "WEAP",
    "ARMR",
    "ALCH",
    "BANK",
    "BOOK",
    "CLOT",
    "FURN",
    "GEMS",
    "LIBR",
    "PAWN",
    "TEMP",
    "TEMP",
    "PALA",
    "FARM",
    "DUNG",
    "CAST",
    "MANR",
    "SHRI",
    "RUIN",
    "SHCK",
    "GRVE",
    "FILL",
    "KRAV",
    "KDRA",
    "KOWL",
    "KMOO",
    "KCAN",
    "KFLA",
    "KHOR",
    "KROS",
    "KWHE",
    "KSCA",
    "KHAW",
    "MAGE",
    "THIE",
    "DARK",
    "FIGH",
    "CUST",
    "WALL",
    "MARK",
    "SHIP",
    "WITC"
};

const char _aszRMBTemple[][3]=
{
    "A0",
    "B0",
    "C0",
    "D0",
    "E0",
    "F0",
    "G0",
    "H0",
};

#if 0 /* unneeded? */
const char _aszRMBChar[][3]=
{
    "AA",
    "AA",
    "AA",
    "AA",
    /*
    "DA",
    "DA",
    "DA",
    "DA",
    "DA",
    */
    "AA",
    "AA",
    "AA",
    "AA",
    "AA",
    //
    "AL",
    "DL",
    "AM",
    "DM",
    "AS",
    "DS",
    "AA",
    "DA",
};
#endif

const char _aszRMBCharQ[][3]=
{
    "AA",
    "BA",
    "AL",
    "BL",
    "AM",
    "BM",
    "AS",
    "BS",
    "GA",
    "GL",
    "GM",
    "GS"
};

} // namespace


uint32_t WorldMap::m_uRegionCount;
std::unique_ptr<Region_Daggerfall[]> WorldMap::m_pRegions;
LocationMap WorldMap::m_MapLoc;
std::map<uint64_t, WorldCell *> WorldMap::m_MapCell;
NameLocationMap WorldMap::m_MapNames;
bool WorldMap::m_bMapLoaded = false;

///////////////////////////////////////////////////////
// Location
///////////////////////////////////////////////////////

Location_Daggerfall::Location_Daggerfall()
{
    m_bLoaded           = false;
    m_dungeonBlockCnt   = 0;
    m_startDungeonBlock = 0;
}

Location_Daggerfall::~Location_Daggerfall()
{
}

void Location_Daggerfall::LoadLoc(const char *pData, int index, const int RegIdx, LocationMap &mapLoc, NameLocationMap &mapNames)
{
    int nPreRecCount = *((const int*)&pData[index]);
    index += 4;

    const uint8_t *PreRecords = nullptr;
    if(nPreRecCount > 0)
    {
        PreRecords = (const uint8_t*)&pData[index];
        index += nPreRecCount*6;
    }
    const LocationRec *pLocation = (const LocationRec*)&pData[index];
    index += sizeof(LocationRec);
    m_LocationID = pLocation->LocationID;
    m_OrigX      = pLocation->XPosition;
    m_OrigY      = pLocation->YPosition;
    m_x          = (float)pLocation->XPosition / 4096.0f;
    m_y          = (float)pLocation->YPosition / 4096.0f;
    strcpy(m_szName, pLocation->LocationName);

    uint64_t uKey = (uint64_t)((int32_t)m_y>>3)<<32ULL |
                    ((uint64_t)((int32_t)m_x>>3)&0xffffffff);
    mapLoc[uKey] = this;

    m_BlockWidth  = 0;
    m_BlockHeight = 0;
    m_pBlockNames = nullptr;

    if ( pLocation->LocType == 0x00008000 )
    {
        //town or exterior location.
        index += 5; //unknown...
        index += 26*pLocation->PostRecCount;    //post records.
        index += 32;    //"Another name"
        /*int locID2 = *((const int*)&pData[index]);*/ index += 4;
        index += 4; //unknowns
        char BlockWidth  = pData[index]; index++;
        char BlockHeight = pData[index]; index++;
        index += 7; //unknowns
        const char *BlockFileIndex  = &pData[index]; index += 64;
        const char *BlockFileNumber = &pData[index]; index += 64;
        const uint8_t *BlockFileChar = (const uint8_t*)&pData[index]; index += 64;
        m_BlockWidth  = BlockWidth;
        m_BlockHeight = BlockHeight;
        m_pBlockNames.reset(new LocName[BlockWidth*BlockHeight]);
        char szName[64];
        for (int by=0; by<BlockHeight; by++)
        {
            for (int bx=0; bx<BlockWidth; bx++)
            {
                int bidx = by*BlockWidth + bx;
                int bfileidx = BlockFileIndex[bidx];
                if ( BlockFileIndex[bidx] == 13 || BlockFileIndex[bidx] == 14 )
                {
                    if ( BlockFileChar[bidx] > 0x07 )
                    {
                        sprintf(szName, "%s%s%s.RMB", _aszRMBHead[bfileidx], "GA", _aszRMBTemple[BlockFileNumber[bidx]&0x07]);
                    }
                    else
                    {
                        sprintf(szName, "%s%s%s.RMB", _aszRMBHead[bfileidx], "AA", _aszRMBTemple[BlockFileNumber[bidx]&0x07]);
                    }

                    if ( !ArchiveManager::GameFile_Open(ARCHIVETYPE_BSA, "BLOCKS.BSA", szName) )
                    {
                        char szNameTmp[32];
                        sprintf(szNameTmp, "%s??%s.RMB", _aszRMBHead[bfileidx], _aszRMBTemple[BlockFileNumber[bidx]&0x07]);
                        ArchiveManager::GameFile_SearchForFile(ARCHIVETYPE_BSA, "BLOCKS.BSA", szNameTmp, szName);
                    }
                    ArchiveManager::GameFile_Close();
                }
                else
                {
                    int Q = BlockFileChar[bidx]/16;
                    if ( BlockFileIndex[bidx] == 40 )   //"CUST"
                    {
                        if ( RegIdx == 20 )  //Sentinel logic
                        {
                            Q = 8;
                        }
                        else
                        {
                            Q = 0;
                        }
                    }
                    else if ( RegIdx == 23 )
                    {
                        if (Q>0) Q--;
                    }
                    sprintf(szName, "%s%s%02d.RMB", _aszRMBHead[bfileidx], _aszRMBCharQ[Q], BlockFileNumber[bidx]);
                    assert(Q < 12);
                    //does this file exist?
                    if ( !ArchiveManager::GameFile_Open(ARCHIVETYPE_BSA, "BLOCKS.BSA", szName) )
                    {
                        char szNameTmp[32];
                        sprintf(szNameTmp, "%s??%02d.RMB", _aszRMBHead[bfileidx], BlockFileNumber[bidx]);
                        ArchiveManager::GameFile_SearchForFile(ARCHIVETYPE_BSA, "BLOCKS.BSA", szNameTmp, szName);
                    }
                    ArchiveManager::GameFile_Close();
                }
                strcpy(m_pBlockNames[bidx].szName, szName);
            }
        }
    }

    //Add to map.
    mapNames[m_szName] = this;
}


///////////////////////////////////////////////////////
// Region
///////////////////////////////////////////////////////
Region_Daggerfall::Region_Daggerfall()
{
}

Region_Daggerfall::~Region_Daggerfall()
{
}

///////////////////////////////////////////////////////
// World Map
///////////////////////////////////////////////////////
void WorldMap::Init()
{
}

void WorldMap::Destroy()
{
    m_bMapLoaded = false;
    m_uRegionCount = 0;
    m_pRegions = nullptr;
}

//load cached data from disk if present.
bool WorldMap::Load()
{
    bool bSuccess = true;
    if(!m_bMapLoaded)
    {
        bSuccess = Cache();
        m_bMapLoaded = true;
    }
    return bSuccess;
}

//generate the cached data.
bool WorldMap::Cache()
{
    //step 1: compute the total number of locations for the entire world...
    char szFileName[64];
    uint32_t TotalLocCount=0;
    ScratchPad::FreeFrame();
    char *pData = (char *)ScratchPad::AllocMem( 1024*1024 );

    //Load global data.
    m_uRegionCount = 62;
    m_pRegions.reset(new Region_Daggerfall[m_uRegionCount]);
    for (int r=0; r<62; r++)
    {
        sprintf(szFileName, "MAPNAMES.0%02d", r);
        if ( ArchiveManager::GameFile_Open(ARCHIVETYPE_BSA, "MAPS.BSA", szFileName) )
        {
            m_pRegions[r].m_uLocationCount = 0;
            m_pRegions[r].m_pLocations = nullptr;

            uint32_t uLength = ArchiveManager::GameFile_GetLength();
            if ( uLength == 0 )
            {
                ArchiveManager::GameFile_Close();
                continue;
            }

            ArchiveManager::GameFile_Read(pData, uLength);
            ArchiveManager::GameFile_Close();

            m_pRegions[r].m_uLocationCount = *((unsigned int *)pData);
            m_pRegions[r].m_pLocations.reset(
                new Location_Daggerfall[m_pRegions[r].m_uLocationCount]);
            TotalLocCount += m_pRegions[r].m_uLocationCount;
        }
    }
    ScratchPad::FreeFrame();

    for (int r=0; r<(int)m_uRegionCount; r++)
    {
        sprintf(szFileName, "MAPTABLE.0%02d", r);
        if ( ArchiveManager::GameFile_Open(ARCHIVETYPE_BSA, "MAPS.BSA", szFileName) )
        {
            uint32_t uLength = ArchiveManager::GameFile_GetLength();
            if ( uLength == 0 )
            {   
                ArchiveManager::GameFile_Close();
                continue;
            }
            char *pData = (char *)ScratchPad::AllocMem(uLength);
            if ( pData )
            {
                ArchiveManager::GameFile_Read(pData, uLength);
                ArchiveManager::GameFile_Close();

                int index = 0;
                for (uint32_t i=0; i<m_pRegions[r].m_uLocationCount; i++)
                {
                    int mapID = *((int *)&pData[index]); index += 4;
                    uint8_t U1 = *((uint8_t *)&pData[index]); index++;
                    uint32_t longType = *((uint32_t *)&pData[index]); index += 4;
                    m_pRegions[r].m_pLocations[i].m_Lat   = ( *((short *)&pData[index]) )&0xFFFF; index += 2;
                    m_pRegions[r].m_pLocations[i].m_Long  = longType&0xFFFF;
                    uint16_t U2 = *((uint16_t *)&pData[index]); index += 2; //unknown
                    uint32_t U3 = *((uint32_t *)&pData[index]); index += 4; //unknown

                    m_pRegions[r].m_pLocations[i].m_locType = longType >> 17;

                }
            }
            ScratchPad::FreeFrame();
        }

        sprintf(szFileName, "MAPPITEM.0%02d", r);
        if ( ArchiveManager::GameFile_Open(ARCHIVETYPE_BSA, "MAPS.BSA", szFileName) )
        {
            uint32_t uLength = ArchiveManager::GameFile_GetLength();
            if ( uLength == 0 )
            {   
                ArchiveManager::GameFile_Close();
                continue;
            }
            char *pData = (char *)ScratchPad::AllocMem(uLength);
            if ( pData )
            {
                ArchiveManager::GameFile_Read(pData, uLength);
                ArchiveManager::GameFile_Close();
                
                //pData = region data.
                uint32_t *offsets = (uint32_t *)pData;
                int nLocationCount = (int)m_pRegions[r].m_uLocationCount;
                int base_index = 4*nLocationCount;
                int index;
                for (int i=0; i<nLocationCount; i++)
                {
                    index = base_index + offsets[i];
                    m_pRegions[r].m_pLocations[i].LoadLoc(pData, index, r, m_MapLoc, m_MapNames);
                }
            }
        }
        ScratchPad::FreeFrame();

        sprintf(szFileName, "MAPDITEM.0%02d", r);
        if ( ArchiveManager::GameFile_Open(ARCHIVETYPE_BSA, "MAPS.BSA", szFileName) )
        {
            uint32_t uLength = ArchiveManager::GameFile_GetLength();
            if ( uLength == 0 )
            {   
                ArchiveManager::GameFile_Close();
                continue;
            }
            char *pData = (char *)ScratchPad::AllocMem(uLength);
            if ( pData )
            {
                ArchiveManager::GameFile_Read(pData, uLength);
                ArchiveManager::GameFile_Close();

                //pData = region data.
                int index = 0;
                uint32_t count = *((uint32_t*)&pData[index]); index += 4;
                struct DungeonOffset
                {
                    uint32_t Offset;
                    uint16_t IsDungeon;
                    uint16_t ExteriorLocID;
                };
                DungeonOffset *pOffsets = (DungeonOffset *)&pData[index]; index += 8*count;
                int base_index = index;
                for (int i=0; i<(int)count; i++)
                {
                    index = base_index + pOffsets[i].Offset;

                    //find matching exterior record...
                    Location_Daggerfall *pCurLoc = nullptr;
                    for (int e=0; e<(int)m_pRegions[r].m_uLocationCount; e++)
                    {
                        if ( m_pRegions[r].m_pLocations[e].m_LocationID == pOffsets[i].ExteriorLocID )
                        {
                            pCurLoc = &m_pRegions[r].m_pLocations[e];
                            break;
                        }
                    }
                    assert(pCurLoc);

                    int nPreRecCount = *((int *)&pData[index]); index += 4;
                    uint8_t *PreRecords = nullptr;
                    if ( nPreRecCount > 0 )
                    {
                        PreRecords = (uint8_t *)&pData[index]; index += nPreRecCount*6;
                    }
                    LocationRec *pLocation = (LocationRec *)&pData[index];
                    index += sizeof(LocationRec);
                    assert( pLocation->LocType == 0 );
                    if ( pLocation->LocType == 0 )
                    {
                        pCurLoc->m_waterHeight = (short)pLocation->Unknown5;
                        if ( pLocation->Unknown3&1 )
                        {
                            pCurLoc->m_waterHeight = -256000;
                        }
                        index += 8;//unknowns...
                        uint16_t blockCnt = *((uint16_t *)&pData[index]); index += 2;
                        index += 5; //unknown

                        pCurLoc->m_dungeonBlockCnt = blockCnt;
                        pCurLoc->m_startDungeonBlock = 0;
                        pCurLoc->m_pDungeonBlocks.reset(
                            new Location_Daggerfall::DungeonBlock[blockCnt]);
                        bool bStartFound = false;
                        for (uint32_t b=0; b<blockCnt; b++)
                        {
                            char X = *((uint8_t *)&pData[index]); index++;
                            char Y = *((uint8_t *)&pData[index]); index++;
                            uint16_t  BlockNumStartIndex = *((uint16_t *)&pData[index]); index+=2;
                            int  blockNum = BlockNumStartIndex&1023;
                            bool IsStartBlock = (BlockNumStartIndex&1024) ? true : false;
                            uint32_t  blockIndex = (BlockNumStartIndex>>11)&0xff;
                            //now decode the string name...
                            char bIdxTable[] = { 'N', 'W', 'L', 'S', 'B', 'M' };
                            char szName[32];
                            sprintf(szName, "%c%07d.RDB", bIdxTable[blockIndex], blockNum);

                            if ( IsStartBlock )
                            {
                                pCurLoc->m_startDungeonBlock = b;
                                bStartFound = true;
                            }

                            strcpy( pCurLoc->m_pDungeonBlocks[b].szName, szName );
                            pCurLoc->m_pDungeonBlocks[b].x = X;
                            pCurLoc->m_pDungeonBlocks[b].y = Y;
                        }
                        assert( bStartFound );
                    }
                }
            }
            ScratchPad::FreeFrame();
        }
    }

    //just to make sure...
    ScratchPad::FreeFrame();
    m_bMapLoaded = true;

    return true;
}

Location_Daggerfall *WorldMap::GetLocation(int32_t x, int32_t y)
{
    uint64_t uKey = ((uint64_t)y<<32ULL) | ((uint64_t)x&0xffffffff);
    auto iLoc = m_MapLoc.find(uKey);
    if(iLoc != m_MapLoc.end())
        return iLoc->second;
    return nullptr;
}

Location_Daggerfall *WorldMap::GetLocation(const char *pszName)
{
    auto iLoc = m_MapNames.find(pszName);
    if(iLoc != m_MapNames.end())
        return iLoc->second;
    return nullptr;
}

void WorldMap::SetWorldCell(int32_t x, int32_t y, WorldCell *pCell)
{
    uint64_t uKey = ((uint64_t)y<<32ULL) | ((uint64_t)x&0xffffffff);
    m_MapCell[uKey] = pCell;
}

WorldCell *WorldMap::GetWorldCell(int32_t x, int32_t y)
{
    uint64_t uKey = ((uint64_t)y<<32ULL) | ((uint64_t)x&0xffffffff);
    auto iLoc = m_MapCell.find(uKey);
    if(iLoc != m_MapCell.end())
        return iLoc->second;
    return nullptr;
}
