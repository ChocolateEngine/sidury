@0xf878a6742b4fd338;

# Base Types
struct Vec3
{
    x @0 : Float32;
    y @1 : Float32;
    z @2 : Float32;
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
    angles       @0 : Vec3;
    buttons      @1 : Int32;
    noclip       @2 : Bool;
}

# General Messages
struct NetMsgServerInfo
{
    # GET THIS OUT OF HERE
    newPort     @0 : Int32;

    protocol    @1 : UInt8;
    name        @2 : Text;
    playerCount @3 : UInt8;
    mapName     @4 : Text;
    mapHash     @5 : Text;
}

struct NetMsgClientInfo
{
    name @0 : Text;
}

struct NetMsgDisconnect
{
    reason @0 : Text;
}

# --------------------------------------------------------
# Components

struct NetEntity
{
    id @0 : UInt32;
}

struct NetCompTransform
{
    pos   @0 : Vec3;
    ang   @1 : Vec3;
    scale @2 : Vec3;
}

struct NetCompTransformSmall
{
    pos   @0 : Vec3;
    ang   @1 : Vec3;
}

struct NetCompCamera
{
    transform @0 : NetCompTransformSmall;
    fov       @1 : Float32;
}

# only some stuff in here is actually networked
struct NetCompPlayerMoveData
{
    moveType        @0 : EPlayerMoveType;
    playerFlags     @1 : Int32;
    prevPlayerFlags @2 : Int32;
    maxSpeed        @3 : Float32;

    # Smooth Duck
    prevViewHeight   @4 : Float32;
    targetViewHeight @5 : Float32;
    outViewHeight    @6 : Float32;
    duckDuration     @7 : Float32;
    duckTime         @8 : Float32;
}

