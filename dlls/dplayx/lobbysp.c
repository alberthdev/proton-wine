/* This contains the implementation of the Lobby Service
 * Providers interface required to communicate with Direct Play
 *
 * Copyright 2001 Peter Hunnisett
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "winerror.h"
#include "wine/debug.h"

#include "lobbysp.h"
#include "dplay_global.h"

WINE_DEFAULT_DEBUG_CHANNEL(dplay);

/* Prototypes */
static BOOL DPLSP_CreateIUnknown( LPVOID lpSP );
static BOOL DPLSP_DestroyIUnknown( LPVOID lpSP );
static BOOL DPLSP_CreateDPLobbySP( void *lpSP, IDirectPlayImpl *dp );
static BOOL DPLSP_DestroyDPLobbySP( LPVOID lpSP );


/* Predefine the interface */
typedef struct IDPLobbySPImpl IDPLobbySPImpl;

typedef struct tagDPLobbySPIUnknownData
{
  CRITICAL_SECTION  DPLSP_lock;
} DPLobbySPIUnknownData;

typedef struct tagDPLobbySPData
{
  IDirectPlayImpl *dplay;
} DPLobbySPData;

#define DPLSP_IMPL_FIELDS \
   DPLobbySPIUnknownData* unk; \
   DPLobbySPData* sp;

struct IDPLobbySPImpl
{
  const IDPLobbySPVtbl *lpVtbl;
  LONG ref;
  DPLSP_IMPL_FIELDS
};

static inline IDPLobbySPImpl *impl_from_IDPLobbySP(IDPLobbySP *iface)
{
    return CONTAINING_RECORD( iface, IDPLobbySPImpl, lpVtbl );
}

/* Forward declaration of virtual tables */
static const IDPLobbySPVtbl dpLobbySPVT;

HRESULT DPLSP_CreateInterface( REFIID riid, void **ppvObj, IDirectPlayImpl *dp )
{
  TRACE( " for %s\n", debugstr_guid( riid ) );

  *ppvObj = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                       sizeof( IDPLobbySPImpl ) );

  if( *ppvObj == NULL )
  {
    return DPERR_OUTOFMEMORY;
  }

  if( IsEqualGUID( &IID_IDPLobbySP, riid ) )
  {
    IDPLobbySPImpl *This = *ppvObj;
    This->lpVtbl = &dpLobbySPVT;
  }
  else
  {
    /* Unsupported interface */
    HeapFree( GetProcessHeap(), 0, *ppvObj );
    *ppvObj = NULL;

    return E_NOINTERFACE;
  }

  /* Initialize it */
  if( DPLSP_CreateIUnknown( *ppvObj ) &&
      DPLSP_CreateDPLobbySP( *ppvObj, dp )
    )
  {
    IDPLobbySP_AddRef( (LPDPLOBBYSP)*ppvObj );
    return S_OK;
  }

  /* Initialize failed, destroy it */
  DPLSP_DestroyDPLobbySP( *ppvObj );
  DPLSP_DestroyIUnknown( *ppvObj );

  HeapFree( GetProcessHeap(), 0, *ppvObj );
  *ppvObj = NULL;

  return DPERR_NOMEMORY;
}

static BOOL DPLSP_CreateIUnknown( LPVOID lpSP )
{
  IDPLobbySPImpl *This = lpSP;

  This->unk = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *(This->unk) ) );

  if ( This->unk == NULL )
  {
    return FALSE;
  }

  InitializeCriticalSection( &This->unk->DPLSP_lock );
  This->unk->DPLSP_lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": IDPLobbySPImpl*->DPLobbySPIUnknownData*->DPLSP_lock");

  return TRUE;
}

static BOOL DPLSP_DestroyIUnknown( LPVOID lpSP )
{
  IDPLobbySPImpl *This = lpSP;

  This->unk->DPLSP_lock.DebugInfo->Spare[0] = 0;
  DeleteCriticalSection( &This->unk->DPLSP_lock );
  HeapFree( GetProcessHeap(), 0, This->unk );

  return TRUE;
}

static BOOL DPLSP_CreateDPLobbySP( void *lpSP, IDirectPlayImpl *dp )
{
  IDPLobbySPImpl *This = lpSP;

  This->sp = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof( *(This->sp) ) );

  if ( This->sp == NULL )
  {
    return FALSE;
  }

  This->sp->dplay = dp;

  return TRUE;
}

static BOOL DPLSP_DestroyDPLobbySP( LPVOID lpSP )
{
  IDPLobbySPImpl *This = lpSP;

  HeapFree( GetProcessHeap(), 0, This->sp );

  return TRUE;
}

static HRESULT WINAPI IDPLobbySPImpl_QueryInterface( IDPLobbySP *iface, REFIID riid,
        void **ppv )
{
  TRACE("(%p)->(%s,%p)\n", iface, debugstr_guid( riid ), ppv );

  if ( IsEqualGUID( &IID_IUnknown, riid ) || IsEqualGUID( &IID_IDPLobbySP, riid ) )
  {
    *ppv = iface;
    IDPLobbySP_AddRef(iface);
    return S_OK;
  }

  FIXME("Unsupported interface %s\n", debugstr_guid(riid));
  *ppv = NULL;
  return E_NOINTERFACE;
}

static ULONG WINAPI IDPLobbySPImpl_AddRef( IDPLobbySP *iface )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  ULONG ref = InterlockedIncrement( &This->ref );

  TRACE( "(%p) ref=%d\n", This, ref );

  return ref;
}

static ULONG WINAPI IDPLobbySPImpl_Release( IDPLobbySP *iface )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  ULONG ref = InterlockedDecrement( &This->ref );

  TRACE( "(%p) ref=%d\n", This, ref );

  if( !ref )
  {
    DPLSP_DestroyDPLobbySP( This );
    DPLSP_DestroyIUnknown( This );
    HeapFree( GetProcessHeap(), 0, This );
  }

  return ref;
}

static HRESULT WINAPI IDPLobbySPImpl_AddGroupToGroup( IDPLobbySP *iface,
        SPDATA_ADDREMOTEGROUPTOGROUP *argtg )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, argtg );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_AddPlayerToGroup( IDPLobbySP *iface,
        SPDATA_ADDREMOTEPLAYERTOGROUP *arptg )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, arptg );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_CreateGroup( IDPLobbySP *iface,
        SPDATA_CREATEREMOTEGROUP *crg )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, crg );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_CreateGroupInGroup( IDPLobbySP *iface,
        SPDATA_CREATEREMOTEGROUPINGROUP *crgig )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, crgig );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_DeleteGroupFromGroup( IDPLobbySP *iface,
        SPDATA_DELETEREMOTEGROUPFROMGROUP *drgfg )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, drgfg );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_DeletePlayerFromGroup( IDPLobbySP *iface,
        SPDATA_DELETEREMOTEPLAYERFROMGROUP *drpfg )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, drpfg );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_DestroyGroup( IDPLobbySP *iface,
        SPDATA_DESTROYREMOTEGROUP *drg )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, drg );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_EnumSessionsResponse( IDPLobbySP *iface,
        SPDATA_ENUMSESSIONSRESPONSE *er )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, er );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_GetSPDataPointer( IDPLobbySP *iface, LPVOID* lplpData )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, lplpData );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_HandleMessage( IDPLobbySP *iface, SPDATA_HANDLEMESSAGE *hm )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, hm );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_SendChatMessage( IDPLobbySP *iface,
        SPDATA_CHATMESSAGE *cm )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, cm );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_SetGroupName( IDPLobbySP *iface,
        SPDATA_SETREMOTEGROUPNAME *srgn )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, srgn );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_SetPlayerName( IDPLobbySP *iface,
        SPDATA_SETREMOTEPLAYERNAME *srpn )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, srpn );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_SetSessionDesc( IDPLobbySP *iface,
        SPDATA_SETSESSIONDESC *ssd )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, ssd );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_SetSPDataPointer( IDPLobbySP *iface, void *lpData )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, lpData );
  return DP_OK;
}

static HRESULT WINAPI IDPLobbySPImpl_StartSession( IDPLobbySP *iface,
        SPDATA_STARTSESSIONCOMMAND *ssc )
{
  IDPLobbySPImpl *This = impl_from_IDPLobbySP( iface );
  FIXME( "(%p)->(%p):stub\n", This, ssc );
  return DP_OK;
}


static const IDPLobbySPVtbl dpLobbySPVT =
{
  IDPLobbySPImpl_QueryInterface,
  IDPLobbySPImpl_AddRef,
  IDPLobbySPImpl_Release,
  IDPLobbySPImpl_AddGroupToGroup,
  IDPLobbySPImpl_AddPlayerToGroup,
  IDPLobbySPImpl_CreateGroup,
  IDPLobbySPImpl_CreateGroupInGroup,
  IDPLobbySPImpl_DeleteGroupFromGroup,
  IDPLobbySPImpl_DeletePlayerFromGroup,
  IDPLobbySPImpl_DestroyGroup,
  IDPLobbySPImpl_EnumSessionsResponse,
  IDPLobbySPImpl_GetSPDataPointer,
  IDPLobbySPImpl_HandleMessage,
  IDPLobbySPImpl_SendChatMessage,
  IDPLobbySPImpl_SetGroupName,
  IDPLobbySPImpl_SetPlayerName,
  IDPLobbySPImpl_SetSessionDesc,
  IDPLobbySPImpl_SetSPDataPointer,
  IDPLobbySPImpl_StartSession
};
