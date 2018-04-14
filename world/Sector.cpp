#include "Sector.h"

#include <algorithm>

uint32_t Sector::s_MaxSecDrawCnt=0;

Sector::Sector()
{
    m_uTypeFlags = SECTOR_TYPE_DUNGEON;
    m_Bounds[0].Set(0.0f, 0.0f, 0.0f);
    m_Bounds[1].Set(0.0f, 0.0f, 0.0f);
    m_x = 0;
    m_y = 0;
    m_bActive = true;
    m_pValidNodes = nullptr;
}

Sector::~Sector()
{
    for (LightObject *light : m_Lights)
    {
        xlDelete light;
    }

    m_Lights.clear();
    xlDelete [] m_pValidNodes;
}

void Sector::AddObject(uint32_t uHandle)
{
    m_Objects.push_back( uHandle );
}

void Sector::RemoveObject(uint32_t uHandle)
{
    //search for object handle and then erase it.
    const auto iter = std::find(m_Objects.begin(), m_Objects.end(), uHandle);

    if (iter != m_Objects.end())
    {
        m_Objects.erase(iter);
    }
}
