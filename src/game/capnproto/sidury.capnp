@0xf878a6742b4fd338;

const chSiduryProtocol :UInt16 = 2;

# Base Types
struct Vec2
{
    x @0 :Float32;
    y @1 :Float32;
}

struct Vec3
{
    x @0 :Float32;
    y @1 :Float32;
    z @2 :Float32;
}

struct Vec4
{
    x @0 :Float32;
    y @1 :Float32;
    z @2 :Float32;
    w @3 :Float32;
}

enum EPlayerMoveType
{
	walk   @0;
	noClip @1;
	fly    @2;
	# water  @3;
}

# --------------------------------------------------------

# User Inputs
struct NetMsgUserCmd
{
    angles       @0 :Vec3;
    buttons      @1 :Int32;
    moveType     @2 :EPlayerMoveType;
    flashlight   @3 :Bool;  # temp
}

# General Messages
struct NetMsgServerInfo
{
    name           @0 :Text;
    clientCount    @1 :UInt8;
    maxClients     @2 :UInt8;  # TODO: make this obsolete with convar syncing?
    mapName        @3 :Text;
    mapHash        @4 :Text;

    # This should probably be a separate message
    clientEntityId @5 :UInt32;
}

struct NetMsgClientInfo
{
    protocol @0 :UInt16;
    name     @1 :Text;
}

struct NetMsgDisconnect
{
    reason @0 :Text;
}

struct NetMsgConVar
{
    command @0 :Text;
}

struct NetMsgPaused
{
    paused @0 :Bool;
}

# --------------------------------------------------------
# Client Messages, Sent to the Server

enum EMsgSrcClient
{
	disconnect @0;
	clientInfo @1;
	conVar     @2;
	userCmd    @3;
	fullUpdate @4;

	count      @5;
}

struct MsgSrcClient
{
    type @0 :EMsgSrcClient;
    data @1 :Data;
}

# --------------------------------------------------------
# Server Messages, Sent to the Client

enum EMsgSrcServer
{
	disconnect    @0;
	serverInfo    @1;
	conVar        @2;
	componentList @3;
	entityList    @4;
	paused        @5;

    count         @6;
}

struct MsgSrcServer
{
    type @0 :EMsgSrcServer;
    data @1 :Data;
}

# --------------------------------------------------------
# Entities and Components

# This contains component data
struct NetMsgComponentUpdate
{
    struct Component
    {
        # Entity to update
        id @0 :UInt32;

        # Is Component Destroyed
        destroyed @1 :Bool;

        # Component Data
        values @2 :Data;
    }

    # Registered Component Name
    name @0 :Text;

    # Entity State
    # state @1 :EState;

    # List of all component data
    components @1 :List(Component);
}

# This only contains updates for entity id's
# And stores whether they are created, destroyed, or unchanged
# We do not network any information about components
struct NetMsgEntityUpdate
{
    # Entity to update
    id @0 :UInt32;

    # Is Entity Destroyed
    destroyed @1 :Bool;
}

struct NetMsgEntityUpdates
{
    # All Entities to update
    updateList  @0 :List(NetMsgEntityUpdate);
}

struct NetMsgComponentUpdates
{
    # All Entities to update
    updateList  @0 :List(NetMsgComponentUpdate);
}

# --------------------------------------------------------
# Entity Components

struct NetCompTransform
{
    pos   @0 :Vec3;
    ang   @1 :Vec3;
    scale @2 :Vec3;
}

struct NetCompTransformSmall
{
    pos   @0 :Vec3;
    ang   @1 :Vec3;
}

struct NetCompRigidBody
{
    vel   @0 :Vec3;
    accel @1 :Vec3;
}

struct NetCompDirection
{
    forward @0 :Vec3;
    up      @1 :Vec3;
    right   @2 :Vec3;
}

struct NetCompGravity
{
    force @0 :Vec3;
}

struct NetCompCamera
{
    direction @0 :NetCompDirection;
    transform @1 :NetCompTransformSmall;
    fov       @2 :Float32;
}

struct NetCompModelPath
{
    path @0 :Text;
}

# only some stuff in here is actually networked
struct NetCompLight
{
    type     @0 :Int32;  # TODO: MAKE AN ENUM
    color    @1 :Vec4;

    # TODO: this should not be here
    # it should be attached to it's own entity that can be parented
    # and that entity needs to contain the transform (or transform small) component
    pos      @2 :Vec3;
    ang      @3 :Vec3;

    innerFov @4 :Float32;
    outerFov @5 :Float32;
    radius   @6 :Float32;
    length   @7 :Float32;

    shadow   @8 :Bool;
    enabled  @9 :Bool;
}

# only some stuff in here is actually networked
struct NetCompPlayerMoveData
{
    moveType        @0 :EPlayerMoveType;
    playerFlags     @1 :UInt8;
    prevPlayerFlags @2 :UInt8;
    maxSpeed        @3 :Float32;

    # Smooth Duck
    prevViewHeight   @4 :Float32;
    targetViewHeight @5 :Float32;
    outViewHeight    @6 :Float32;
    duckDuration     @7 :Float32;
    duckTime         @8 :Float32;
}

