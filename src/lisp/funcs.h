#ifndef FUNCS_H
#define FUNCS_H

enum class SysFunc
{
    Print,
    Car,
    Cdr,
    Length,
    List,
    Cons,
    Quote,
    Eq,
    Plus,
    Minus,
    If,
    Setf,
    SymbolList,
    Assoc,
    Null,
    Acons,
    Pairlis,
    Let,
    Defun,
    Atom,
    Not,
    And,
    Or,
    Progn,
    Equal,
    Concatenate,
    CharCode,
    CodeChar,
    Times,
    Slash,
    Cond,
    Select,
    Function,
    Mapcar,
    Funcall,
    GreaterThan,
    LessThan,
    TmpSpace,
    PermSpace,
    SymbolName,
    Trace,
    Untrace,
    Digstr,
    CompileFile,
    Abs,
    Min,
    Max,
    GreaterOrEqual,
    LessOrEqual,
    Backquote,
    Comma,
    Nth,
    ResizeTmp,
    ResizePerm,
    Cos,
    Sin,
    Atan2,
    Enum,
    Quit,
    Eval,
    Break,
    Mod,
    WriteProfile,
    Setq,
    For,
    OpenFile,
    Load,
    BitAnd,
    BitOr,
    BitXor,
    MakeArray,
    Aref,
    If1Progn,
    If2Progn,
    If12Progn,
    Eq0,
    Preport,
    Search,
    Elt,
    Listp,
    Numberp,
    Do,
    GarbageCollect,
    Schar,
    Symbolp,
    Num2Str,
    Nconc,
    First,
    Second,
    Third,
    Fourth,
    Fifth,
    Sixth,
    Seventh,
    Eighth,
    Ninth,
    Tenth,
    Substr,
    LocalLoad
};

enum class CFunc
{
    DistX,
    DistY,
    KeyPressed,
    LocalKeyPressed,
    BgState,
    AiType,
    AiState,
    SetAiState,
    Random,
    StateTime,
    State,
    Toward,
    Move,
    Facing,
    OType,
    NextPicture,
    SetFadeDir,
    Mover,
    SetFadeCount,
    FadeCount,
    FadeDir,
    TouchingBg,
    AddPower,
    AddHp,
    Draw,
    EditMode,
    DrawAbove,
    X,
    Y,
    SetX,
    SetY,
    SetLastX,
    SetLastY,
    PushCharacters,
    SetState,
    BgX,
    BgY,
    SetAiType,
    XVel,
    YVel,
    SetXVel,
    SetYVel,
    Away,
    BlockedLeft,
    BlockedRight,
    AddPalette, // name, w,h,x,y,scale, tiles
    ScreenShot, // filename
    SetZoom,
    ShowHelp, // type, x,y
    Direction,
    SetDirection,
    FreezePlayer, // freeze time
    DoCommand, // command string
    SetGameState,
    // scene control functions, game must first be set to scene mode.
    SceneSetTextRegion,
    SceneSetFrameSpeed,
    SceneSetScrollSpeed,
    SceneSetPanSpeed,
    SceneScrollText,
    ScenePan,
    SceneWait,
    LevelNew, // width, height, name
    DoDamage, // amount, to_object, [pushx pushy]
    Hp,
    SetShiftDown,
    SetShiftRight,
    SetGravity,
    Tick,
    SetXacel,
    SetYacel,
    SetLocalPlayers, // set # of players on this machine, unsupported?
    LocalPlayers,
    SetLightDetail,
    LightDetail,
    SetMorphDetail,
    MorphDetail,
    MorphInto, // type aneal frames
    LinkObject,
    DrawLine,
    DrawLaser,
    DarkColor,
    MediumColor,
    BrightColor,
    RemoveObject,
    LinkLight,
    RemoveLight,
    TotalObjects,
    TotalLights,
    SetLightR1,
    SetLightR2,
    SetLightX,
    SetLightY,
    SetLightXShift,
    SetLightYShift,
    LightR1,
    LightR2,
    LightX,
    LightY,
    LightXShift,
    LightYShift,
    Xacel,
    Yacel,
    DeleteLight,
    SetFx,
    SetFy,
    SetFxvel,
    SetFyvel,
    SetFxacel,
    SetFyacel,
    PictureWidth,
    PictureHeight,
    Trap,
    PlatformPush,
    DefSound, // symbol, filename [ or just filenmae]
    PlaySound,
    DefParticle, // symbol, filename
    AddPanim, // id, x, y, dir
    WeaponToType, // returns total for type weapon
    HurtRadius, // x y radius max_damage exclude_object max_push
    AddAmmo, // weapon_type, amount
    AmmoTotal, // returns total for type weapon
    CurrentWeapon, // weapon_type, amount
    CurrentWeaponType, // returns total for type weapon
    BlockedUp,
    BlockedDown,
    GiveWeapon, // type
    GetAbility,
    ResetPlayer,
    SiteAngle,
    SetCourse, // angle, magnitude
    SetFrameAngle, // ang1,ang2, ang
    JumpState, // don't reset current_frame
    Morphing,
    DamageFun,
    Gravity,
    MakeViewSolid,
    FindRgb,
    PlayerXSuggest, // return player "joystick" x
    PlayerYSuggest,
    PlayerB1Suggest,
    PlayerB2Suggest,
    PlayerB3Suggest,
    SetBgScroll, // xmul xdiv ymul ydiv
    SetAmbientLight, // player, 0..63 (out of bounds ignored)
    AmbientLight, // player
    HasObject, // true if linked with object x
    SetOtype, // otype
    CurrentFrame,
    Fx,
    Fy,
    Fxvel,
    Fyvel,
    Fxacel,
    Fyacel,
    SetStatBar, // filename, object
    SetFgTile, // x,y, tile #
    FgTile, // x,y
    SetBgTile, // x,y, tile #
    BgTile, // x,y
    LoadTiles, // filename1, filename2...
    LoadPalette, // filename
    LoadColorFilter, // filename
    CreatePlayers, // player type, returns true if game is networked
    TryMove, // xv yv (check_top=t) -> returns T if not blocked
    SequenceLength, // sequence number
    CanSee, // x1,y1, x2,y2, chars_block
    LoadBigFont, // filename, name
    LoadSmallFont, // filename, name
    LoadConsoleFont, // filename, name
    SetCurrentFrame,
    DrawTransparent, // count, max
    DrawTint, // tint id number
    DrawPredator, // tint_number
    ShiftDown, // player
    ShiftRight, // player
    SetNoScrollRange, // player, x1,y1,x2,y2
    DefImage, // filename,name
    PutImage, // x,y,id
    ViewX1,
    ViewY1,
    ViewX2,
    ViewY2,
    ViewPushDown,
    LocalPlayer,
    SaveGame, // filename
    SetHp,
    RequestLevelLoad, // filename
    SetFirstLevel, // filename
    DefTint, // filename
    TintPalette, // radd,gadd,badd
    PlayerNumber,
    SetCurrentWeapon, // type
    HasWeapon, // type
    AmbientRamp,
    TotalPlayers,
    ScatterLine, // x1,y1,x2,y2, color, scatter value
    GameTick,
    IsaPlayer,
    ShiftRandTable, // amount
    TotalFrames,
    Raise, // call only from reload constructor!
    Lower, // call only from reload constructor!
    PlayerPointerX,
    PlayerPointerY,
    FramePanic,
    AscatterLine, // x1,y1,x2,y2, color1, color2, scatter value
    RandOn,
    SetRandOn,
    Bar,
    Argc,
    PlaySong, // filename
    StopSong,
    Targetable,
    SetTargetable, // T or nil
    ShowStats,
    Kills,
    TKills,
    Secrets,
    TSecrets,
    SetKills,
    SetTKills,
    SetSecrets,
    SetTSecrets,
    RequestEndGame,
    GetSaveSlot,
    MemReport,
    MajorVersion,
    MinorVersion,
    DrawDoubleTint, // tint1 id number, tint 2 id number
    ImageWidth, // image number
    ImageHeight, // image number
    ForegroundWidth,
    ForegroundHeight,
    BackgroundWidth,
    BackgroundHeight,
    GetKeyCode, // name of key, returns code that can be used with keypressed
    SetCursorShape, // image id, x hot, y hot
    StartServer,
    PutString, // font,x,y,string, [color]
    FontWidth, // font
    FontHeight, // font
    ChatPrint, // chat string
    SetPlayerName, // name
    DrawBar, // x1,y1,x2,y2,color
    DrawRect, // x1,y1,x2,y2,color
    GetOption,
    SetDelayOn, // T or nil
    SetLogin, // name
    EnableChatting,
    DemoBreakEnable,
    AmAClient,
    TimeForNextLevel,
    ResetKills,
    SetGameName, // server game name
    SetNetMinPlayers,
    SetObjectTint,
    GetObjectTint,
    SetObjectTeam,
    GetObjectTeam,
};

enum class LispFunc
{
    GoState,
    WithObject,
    BMove, // returns true=unblocked, nil=block, or object
    Me,
    Bg,
    FindClosest,
    FindXClosest,
    FindXRange,
    AddObject, // type, x,y (type)
    FirstFocus,
    NextFocus,
    GetObject,
    GetLight,
    WithObjects,
    AddLight, // type, x, y, r1, r2, xshift, yshift
    FindEnemy, // exclude
    UserFun, // calls anobject's user function
    Time, // returns a fixed point (times and operation)
    Name, // returns current object's name (for debugin)
    FloatTick,
    FindObjectInArea, // x1, y1, x2, y2  type_list
    FindObjectInAngle, // start_angle end_angle type_list
    AddObjectAfter, // type, x,y (type)
    DefChar, // needs at least 2 parms
    SeeDist, // returns longest unblocked path from x1,y1,x2,y2
    Platform,
    LevelName,
    AntAi,
    SensorAi,
    DevDraw,
    TopAi,
    LaserUFun,
    TopUFun,
    PLaserUFun,
    PlayerRocketUFun,
    LSaberUFun,
    CopMover,
    LatterAi,
    WithObj0,
    Activated,
    TopDraw,
    BottomDraw,
    MoverAi,
    SGunAi,
    LastSavegameName,
    NextSavegameName,
    Argv,
    JoyStat, // xm ym b1 b2 b3
    MouseStat, // mx my b1 b2 b3
    MouseToGame, // pass in x,y -> x,y
    GameToMouse, // pass in x,y -> x,y
    GetMainFont,
    PlayerName,
    GetCwd,
    System,
    ConvertSlashes,
    GetDirectory, // path
    RespawnAi,
    ScoreDraw,
    ShowKills,
    MkPtr,
    Seq,
};

extern void initSysFuncs();
extern void initCFuncs();
extern void initLispFuncs();

#endif // FUNCS_H
