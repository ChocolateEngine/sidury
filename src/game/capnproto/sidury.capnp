@0xf878a6742b4fd338;

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
    # GET THIS OUT OF HERE
    newPort        @0 :Int32;

    protocol       @1 :UInt8;
    name           @2 :Text;
    playerCount    @3 :UInt8;
    mapName        @4 :Text;
    mapHash        @5 :Text;

    playerEntityId @6 :UInt32;
}

struct NetMsgClientInfo
{
    name @0 :Text;
}

struct NetMsgDisconnect
{
    reason @0 :Text;
}

struct NetMsgConVar
{
    command @0 :Text;
}

# --------------------------------------------------------
# Client Messages, Sent to the Server

enum EMsgSrcClient
{
	disconnect @0;
	clientInfo @1;
	conVar     @2;
	userCmd    @3;
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
	disconnect @0;
	serverInfo @1;
	conVar     @2;
	entityList @3;
}

struct MsgSrcServer
{
    type @0 :EMsgSrcServer;
    data @1 :Data;
}

# --------------------------------------------------------
# Entities

struct NetMsgEntity
{
    id @0 :UInt32;
}

struct NetMsgEntityUpdate
{
    enum EState
    {
        none      @0;
        created   @1;
        destroyed @2;
    }

    struct Component
    {
        name   @0 :Text;    # Registered Component Name
        values @1 :Data;    # Component Data
    }

    # Entity to update
    id @0 :UInt32;

    # Entity State
    # state @1 :EState;

    # List of all component data
    components @1 :List(Component);
}

struct NetMsgEntityUpdates
{
    # All Entities to update
    updateList  @0 :List(NetMsgEntityUpdate);
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

