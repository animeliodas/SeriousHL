/* SeriousHL plugin precompiled header */
#ifndef SERIOUSHL_STDH_H
#define SERIOUSHL_STDH_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <Core/Core.h>

#include <Engine/Brushes/Brush.h>
#include <Engine/World/WorldRayCasting.h>

// this port to the SeriousBridge core (core/sam/SBridge.h): its log tag and temp folder (dev hook commands, test trace)
#define SB_NAME "SeriousHL"

#endif
