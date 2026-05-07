//using System;
//using System.IO;
//using System.Reflection;
//using System.Text;
//using HarmonyLib;
//using MelonLoader;
//using UnityEngine;
//using UnityEngine.InputSystem;

//[assembly: MelonInfo(typeof(GalleryUnlocker.MelonLoader.Core), "GalleryUnlocker", "1.0.0", "User")]
//[assembly: MelonGame("NosyFellow", "NTRMobile")]
//[assembly: MelonPlatformDomain(MelonPlatformDomainAttribute.CompatibleDomains.IL2CPP)]

//namespace GalleryUnlocker.MelonLoader
//{
//    public class Core : MelonMod
//    {
//        private static bool _isUnlocked = false;
//        private static string _configPath;
//        private static string _imagesPath;
//        private static string _imagesPersistentPath;

//        private bool _wasPressed = false;
//        private bool _configLoaded = false;

//        public static bool IsUnlocked => _isUnlocked;

//        private static MethodInfo _dialogueLuaSetVariable;

//        private const string ALL_IMAGES = "Liz_P1,Liz_P2,Liz_P3,Liz_P4,Liz_P5,Liz_P6,Liz_P7,Liz_P8,Liz_P9,Liz_P10," +
//            "Liz_P11,Liz_P12,Liz_P13,Liz_P14,Liz_P15,Liz_P16,Liz_P17,Liz_P18,Liz_P19,Liz_P20," +
//            "Liz_P21,Liz_P22,Liz_P23,Liz_P24,Liz_P25,Liz_P26,Liz_P27,Liz_P28,Liz_P29,Liz_P30," +
//            "Liz_P31,Liz_P32,Liz_P33,Liz_P34,Liz_P35,Liz_P36,Liz_P37,Liz_P38,Liz_P39,Liz_P40," +
//            "Liz_P41,Liz_P42,Liz_P43,Liz_P44,Liz_P45,Liz_P46,Liz_P47,Liz_P48,Liz_P49,Liz_P50," +
//            "Liz_P51,Liz_P52,Liz_P53,Liz_P54,Liz_P55,Liz_P56,Liz_P57,Liz_P58,Liz_P59,Liz_P60," +
//            "Liz_P61,Liz_P62,Liz_P63,Liz_P64,Liz_P65,Liz_P66,Liz_P67,Liz_P68,Liz_P69,Liz_P70," +
//            "Liz_P71,Liz_P72,Liz_P73,Liz_P74,Liz_P75,Liz_P76,Liz_P77,Liz_P78,Liz_P79,Liz_P80," +
//            "Liz_P81,Liz_P82,Liz_P83,Liz_P84,Liz_P85,Liz_P86,Liz_P87,Liz_P88,Liz_P89,Liz_P90," +
//            "Liz_P91,Liz_P92,Liz_P93,Liz_P94,Liz_P95,Liz_P96,Liz_P97,Liz_P98,Liz_P99,Liz_P100," +
//            "Liz_P101,Liz_P102,Liz_P103,Liz_P104,Liz_P105,Liz_P106,Liz_P107,Liz_P108,Liz_P109,Liz_P110," +
//            "Liz_P112,Liz_P113,Liz_P114,Liz_P115,Liz_P116,Liz_P117,Liz_P118,Liz_P119,Liz_P120," +
//            "Liz_P121,Liz_P122,Liz_P123,Liz_P124,Liz_P125,Liz_P126,Liz_P127,Liz_P128,Liz_P129,Liz_P130," +
//            "Liz_P131,Liz_P132,Liz_P133,Liz_P134,Liz_P135,Liz_P136,Liz_P137,Liz_P138,Liz_P139,Liz_P140," +
//            "Liz_P141,Liz_P142,Liz_P143,Liz_P144,Liz_P145,Liz_P146,Liz_P147,Liz_P148,Liz_P149,Liz_P150," +
//            "Liz_P151,Liz_P152,Liz_P153,Liz_P154,Liz_P155,Liz_P156,Liz_P157,Liz_P158,Liz_P159,Liz_P160," +
//            "Liz_P161,Liz_P162,Liz_P163,Liz_P164,Liz_P165,Liz_P166,Liz_P167,Liz_P168,Liz_P169,Liz_P170," +
//            "Liz_P171,Liz_P172,Liz_P173,Liz_P174,Liz_P175,Liz_P176,Liz_P177,Liz_P178,Liz_P179,Liz_P180," +
//            "Liz_P181,Liz_P182,Liz_P183,Liz_P184,Liz_P185,Liz_P186,Liz_P187,Liz_P188,Liz_P189,Liz_P190," +
//            "Liz_P191,Liz_P192,Liz_P193,Liz_P194,Liz_P195,Liz_P196,Liz_P197,Liz_P198,Liz_P199,Liz_P200," +
//            "Liz_P201,Liz_P202,Liz_P203,Liz_P204,Liz_P205,Liz_P206,Liz_P207,Liz_P208,Liz_P209,Liz_P210," +
//            "Liz_P211,Liz_P212,Liz_P213,Liz_P214,Liz_P215,Liz_P216,Liz_P217,Liz_P218,Liz_P219,Liz_P220," +
//            "Liz_P221,Liz_P222,Liz_P223,Liz_P224,Liz_P225,Liz_P226,Liz_P227,Liz_P228,Liz_P229,Liz_P230," +
//            "Liz_P231,Liz_P232,Liz_P233,Liz_P234,Liz_P235,Liz_P236,Liz_P237,Liz_P238,Liz_P239,Liz_P240," +
//            "Liz_P241,Liz_P242,Liz_P243,Liz_P244,Liz_P245,Liz_P246,Liz_P247,Liz_P248,Liz_P249,Liz_P250," +
//            "Liz_P251,Liz_P252,Liz_P253,Liz_P254,Liz_P255,Liz_P256,Liz_P257,Liz_P258,Liz_P259,Liz_P260," +
//            "Liz_P261,Liz_P262,Liz_P263,Liz_P264,Liz_P265,Liz_P266,Liz_P267,Liz_P268,Liz_P269,Liz_P270," +
//            "Liz_P271,Liz_P272,Liz_P273,Liz_P274,Liz_P275,Liz_P276,Liz_P277,Liz_P278,Liz_P279,Liz_P280," +
//            "Liz_P281,Liz_P282,Liz_P283,Liz_P284,Liz_P285,Liz_P286,Liz_P287,Liz_P288,Liz_P289,Liz_P290," +
//            "Liz_P301,Liz_P302,Liz_P303,Liz_P304,Liz_P305,Liz_P306,Liz_P307,Liz_P308,Liz_P309,Liz_P310," +
//            "Liz_P311,Liz_P312,Liz_P313,Liz_P314,Liz_P315,Liz_P316,Liz_P317,Liz_P318,Liz_P319,Liz_P320," +
//            "Liz_P321,Liz_P322,Liz_P323,Liz_P324,Liz_P325,Liz_P326,Liz_P327,Liz_P328,Liz_P329,Liz_P330," +
//            "Liz_P331,Liz_P332,Liz_P333,Liz_P334,Liz_P335,Liz_P336,Liz_P337,Liz_P338,Liz_P339,Liz_P340," +
//            "Liz_P341,Liz_P342,Liz_P343,Liz_P344,Liz_P345,Liz_P346,Liz_P347,Liz_P348,Liz_P349,Liz_P350," +
//            "Liz_P351,Liz_P352,Liz_P353,Liz_P354,Liz_P355,Liz_P356,Liz_P357,Liz_P358,Liz_P359,Liz_P360," +
//            "Liz_P361,Liz_P362,Liz_P363,Liz_P364,Liz_P365,Liz_P366,Liz_P367,Liz_P368,Liz_P369,Liz_P370," +
//            "Liz_P371,Liz_P372,Liz_P373,Liz_P374,Liz_P375,Liz_P376,Liz_P377,Liz_P378,Liz_P379,Liz_P380," +
//            "Liz_P381,Liz_P382,Liz_P383,Liz_P384,Liz_P385,Liz_P386,Liz_P387,Liz_P388,Liz_P389,Liz_P390," +
//            "Liz_P391,Liz_P392,Liz_P393,Liz_P394,Liz_P395,Liz_P396,Liz_P397,Liz_P398,Liz_P399,Liz_P400," +
//            "Liz_P401,Liz_P402,Liz_P403,Liz_P404,Liz_P405,Liz_P406,Liz_P407,Liz_P408,Liz_P409,Liz_P410," +
//            "Liz_P411,Liz_P412,Liz_P413,Liz_P414,Liz_P415,Liz_P416,Liz_P417,Liz_P418,Liz_P419,Liz_P420," +
//            "Liz_P421,Liz_P422,Liz_P423,Liz_P424,Liz_P425,Liz_P426,Liz_P427,Liz_P428,Liz_P429,Liz_P430," +
//            "Liz_P431,Liz_P432,Liz_P433,Liz_P434,Liz_P435,Liz_P436,Liz_P437,Liz_P438,Liz_P439,Liz_P440," +
//            "Liz_P441,Liz_P442,Liz_P443,Liz_P444,Liz_P445,Liz_P446,Liz_P447,Liz_P448,Liz_P449,Liz_P450," +
//            "Liz_P451,Liz_P452,Liz_P453,Liz_P454,Liz_P455,Liz_P456,Liz_P457,Liz_P458,Liz_P459," +
//            "Sam_P1,Sam_P2,Sam_P3,Sam_P4,Sam_P5,Sam_P6,Sam_P7,Sam_P8,Sam_P9,Sam_P10," +
//            "Sam_P11,Sam_P12,Sam_P13,Sam_P14,Sam_P15,Sam_P16,Sam_P17,Sam_P18,Sam_P19,Sam_P20," +
//            "Sam_P21,Sam_P22,Sam_P23,Sam_P24,Sam_P25,Sam_P26,Sam_P27,Sam_P28,Sam_P29,Sam_P30," +
//            "Sam_P31,Sam_P32,Sam_P33,Sam_P34,Sam_P35,Sam_P36,Sam_P37,Sam_P38,Sam_P39,Sam_P40," +
//            "Sam_P41,Sam_P42,Sam_P43,Sam_P44,Sam_P45,Sam_P46,Sam_P47,Sam_P48,Sam_P49,Sam_P50," +
//            "Sam_P51,Sam_P52,Sam_P53,Sam_P54,Sam_P55,Sam_P56,Sam_P57,Sam_P58,Sam_P59,Sam_P60," +
//            "Sam_P61,Sam_P62,Sam_P63,Sam_P64,Sam_P65,Sam_P66,Sam_P67,Sam_P68,Sam_P69,Sam_P70," +
//            "Sam_P71,Sam_P72,Sam_P73,Sam_P74,Sam_P75,Sam_P76,Sam_P77,Sam_P78,Sam_P79,Sam_P80," +
//            "Sam_P81,Sam_P82,Sam_P83,Sam_P84,Sam_P85,Sam_P86,Sam_P87," +
//            "Catherine_P1,Catherine_P2,Catherine_P3,Catherine_P4," +
//            "Ishani_P1,Ishani_P2,Ishani_P3,Ishani_P4,Ishani_P5,Ishani_P6,Ishani_P7,Ishani_P8," +
//            "Mtr_P0,Mtr_P1,Mtr_P2,Mtr_P3,Mtr_P4,Mtr_P5,Mtr_P6,Mtr_P7,Mtr_P8,Mtr_P9,Mtr_P10," +
//            "Mtr_P11,Mtr_P12,Mtr_P12v,Mtr_P13,Mtr_P14,Mtr_P15,Mtr_P16,Mtr_P17,Mtr_P18,Mtr_P19,Mtr_P20," +
//            "Mtr_P21,Mtr_P22,Mtr_P23,Mtr_P24,Mtr_P25,Mtr_P26,Mtr_P27,Mtr_P28,Mtr_P29,Mtr_P30," +
//            "Mtr_P31,Mtr_P32,Mtr_P33,Mtr_P34,Mtr_P35,Mtr_P36,Mtr_P37,Mtr_P38,Mtr_P39,Mtr_P40," +
//            "Mtr_P41,Mtr_P42,Mtr_P43,Mtr_P44,Mtr_P45,Mtr_P46,Mtr_P47,Mtr_P48,Mtr_P49,Mtr_P50," +
//            "Mtr_P51,Mtr_P52,Mtr_P53,Mtr_P54,Mtr_P55,Mtr_P56,Mtr_P57,Mtr_P58,Mtr_P59,Mtr_P60," +
//            "Mtr_P61,Mtr_P62,Mtr_P63,Mtr_P64,Mtr_P65,Mtr_P66,Mtr_P67,Mtr_P68,Mtr_P69,Mtr_P70," +
//            "Mtr_P71,Mtr_P72,Mtr_P73,Mtr_P74,Mtr_P75,Mtr_P76,Mtr_P77,Mtr_P78,Mtr_P79,Mtr_P80," +
//            "Mtr_P81,Mtr_P82,Mtr_P83,Mtr_P84,Mtr_P85,Mtr_P86,Mtr_P87,Mtr_P88,Mtr_P89,Mtr_P90," +
//            "Mtr_P91,Mtr_P92,Mtr_P93,Mtr_P94,Mtr_P95,Mtr_P96,Mtr_P97,Mtr_P98,Mtr_P99,Mtr_P100," +
//            "Mtr_P101,Mtr_P102,Mtr_P103,Mtr_P104,Mtr_P105,Mtr_P106,Mtr_P107,Mtr_P108,Mtr_P109,Mtr_P110," +
//            "Mtr_P111,Mtr_P112,Mtr_P113,Mtr_P114,Mtr_P115,Mtr_P116,Mtr_P117,Mtr_P118,Mtr_P119,Mtr_P120," +
//            "Mtr_P121,Mtr_P122,Mtr_P123,Mtr_P124,Mtr_P125,Mtr_P126,Mtr_P127,Mtr_P128,Mtr_P129,Mtr_P130," +
//            "Mtr_P131,Mtr_P132,Mtr_P133,Mtr_P134,Mtr_P135,Mtr_P136,Mtr_P137,Mtr_P138,Mtr_P139,Mtr_P140," +
//            "Mtr_P141,Mtr_P142,Mtr_P143,Mtr_P144,Mtr_P145,Mtr_P146,Mtr_P147,Mtr_P148,Mtr_P149,Mtr_P150," +
//            "Mtr_P151,Mtr_P152,Mtr_P153,Mtr_P154,Mtr_P155,Mtr_P156,Mtr_P157,Mtr_P158,Mtr_P159,Mtr_P160," +
//            "Mtr_P161,Mtr_P162,Mtr_P163,Mtr_P164,Mtr_P165,Mtr_P166,Mtr_P167,Mtr_P168,Mtr_P169,Mtr_P170," +
//            "Mtr_P171,Mtr_P172,Mtr_P173,Mtr_P174,Mtr_P175,Mtr_P176,Mtr_P177,Mtr_P178,Mtr_P179,Mtr_P180," +
//            "Mtr_P181,Mtr_P182,Mtr_P183,Mtr_P184,Mtr_P185,Mtr_P186,Mtr_P187,Mtr_P188,Mtr_P189,Mtr_P190," +
//            "Mtr_P191,Mtr_P192,Mtr_P193,Mtr_P194,Mtr_P195,Mtr_P196,Mtr_P197,Mtr_P198,Mtr_P199,Mtr_P200," +
//            "Mtr_P201,Mtr_P202,Mtr_P203,Mtr_P204,Mtr_P205,Mtr_P206,Mtr_P207,Mtr_P208,Mtr_P209,Mtr_P210," +
//            "Mtr_P211,Mtr_P212,Mtr_P213,Mtr_P214,Mtr_P215,Mtr_P216,Mtr_P217,Mtr_P218,Mtr_P219,Mtr_P220," +
//            "Mtr_P221,Mtr_P222,Mtr_P223,Mtr_P224,Mtr_P225,Mtr_P226,Mtr_P227,Mtr_P228,Mtr_P229,Mtr_P230," +
//            "Mtr_P231,Mtr_P232,Mtr_P233,Mtr_P234,Mtr_P235,Mtr_P236,Mtr_P237,Mtr_P238,Mtr_P239,Mtr_P240," +
//            "Mtr_P241,Mtr_P242,Mtr_P243,Mtr_P244,Mtr_P245,Mtr_P246,Mtr_P247,Mtr_P248,Mtr_P249,Mtr_P250," +
//            "Mtr_P251,Mtr_P252,Mtr_P253,Mtr_P254,Mtr_P255,Mtr_P256,Mtr_P257,Mtr_P258,Mtr_P259,Mtr_P260," +
//            "Mtr_P261,Mtr_P262,Mtr_P263,Mtr_P264,Mtr_P265,Mtr_P266,Mtr_P267,Mtr_P268,Mtr_P269,Mtr_P270," +
//            "Mtr_P271,Mtr_P272,Mtr_P273,Mtr_P274,Mtr_P275,Mtr_P276,Mtr_P277,Mtr_P278,Mtr_P279,Mtr_P280," +
//            "Mtr_P281,Mtr_P282,Mtr_P283,Mtr_P284,Mtr_P285,Mtr_P286,Mtr_P287,Mtr_P288,Mtr_P289,Mtr_P290," +
//            "Mtr_P301,Mtr_P302,Mtr_P303,Mtr_P304,Mtr_P305,Mtr_P306,Mtr_P307,Mtr_P308,Mtr_P309,Mtr_P310," +
//            "Mtr_P311,Mtr_P312,Mtr_P313,Mtr_P314,Mtr_P315,Mtr_P316,Mtr_P317,Mtr_P318,Mtr_P319,Mtr_P320," +
//            "Mtr_P321,Mtr_P322,Mtr_P323,Mtr_P324,Mtr_P325,Mtr_P326,Mtr_P327,Mtr_P328,Mtr_P329,Mtr_P330," +
//            "Mtr_P331,Mtr_P332,Mtr_P333,Mtr_P334,Mtr_P335,Mtr_P336,Mtr_P337,Mtr_P338,Mtr_P339,Mtr_P340," +
//            "Mtr_P341,Mtr_P342,Mtr_P343,Mtr_P344,Mtr_P345,Mtr_P346,Mtr_P347,Mtr_P348,Mtr_P349,Mtr_P350," +
//            "Mtr_P351,Mtr_P352,Mtr_P353,Mtr_P354,Mtr_P355,Mtr_P356,Mtr_P357,Mtr_P358,Mtr_P359,Mtr_P360," +
//            "Mtr_P361,Mtr_P362,Mtr_P363,Mtr_P364,Mtr_P365,Mtr_P366,Mtr_P367,Mtr_P368,Mtr_P369,Mtr_P370," +
//            "Mtr_P371,Mtr_P372,Mtr_P373,Mtr_P374,Mtr_P375,Mtr_P376,Mtr_P377,Mtr_P378,Mtr_P379,Mtr_P380," +
//            "Mtr_P381,Mtr_P382,Mtr_P383,Mtr_P384,Mtr_P385,Mtr_P386,Mtr_P387,Mtr_P388,Mtr_P389,Mtr_P390," +
//            "Mtr_P391,Mtr_P392,Mtr_P393,Mtr_P394,Mtr_P395,Mtr_P396,Mtr_P397,Mtr_P398,Mtr_P399,Mtr_P400," +
//            "Mtr_P401,Mtr_P402,Mtr_P403,Mtr_P404,Mtr_P405,Mtr_P406,Mtr_P407,Mtr_P408,Mtr_P409,Mtr_P410," +
//            "Mtr_P411,Mtr_P412,Mtr_P413,Mtr_P414,Mtr_P415,Mtr_P416,Mtr_P417,Mtr_P418,Mtr_P419,Mtr_P420," +
//            "Mtr_P421,Mtr_P422,Mtr_P423,Mtr_P424,Mtr_P425,Mtr_P426,Mtr_P427,Mtr_P428,Mtr_P429,Mtr_P430," +
//            "Mtr_P431,Mtr_P432,Mtr_P433,Mtr_P434,Mtr_P435,Mtr_P436,Mtr_P437,Mtr_P438,Mtr_P439,Mtr_P440," +
//            "Mtr_P441,Mtr_P442,Mtr_P443,Mtr_P444,Mtr_P445,Mtr_P446,Mtr_P447,Mtr_P448,Mtr_P449,Mtr_P450," +
//            "Wall_P1,Wall_P2,Wall_P3,Wall_P4,Wall_P5,";

//        private const string ALL_COMICS = "Liz_CS_1,Liz_CS_2,Liz_CS_3,Liz_CS_4,Liz_CS_5,Liz_CS_6,Liz_CS_7,Liz_CS_8,Liz_CS_9,Liz_CS_10," +
//            "Liz_CS_12,Liz_CS_13,Liz_CS_14,Liz_CS_15,Liz_CS_16,Liz_CS_17,Liz_CS_18,Liz_CS_19,Liz_CS_20," +
//            "Liz_CS_21,Liz_CS_22,Liz_CS_24,Liz_CS_25,Liz_CS_26,Liz_CS_27,Liz_CS_28,Liz_CS_29,Liz_CS_30," +
//            "Liz_CS_31,Liz_CS_32,Liz_CS_33,Liz_CS_34,Liz_CS_35,Liz_CS_36,Liz_CS_38,Liz_CS_39,Liz_CS_40," +
//            "Liz_CS_41,Liz_CS_43,Liz_CS_45,Liz_CS_48," +
//            "Sam_CS_1,Sam_CS_2,Sam_CS_3,Sam_CS_5,Sam_CS_6," +
//            "Rich_CS_1,Rich_CS_2,Rich_CS_5,Rich_CS_6";

//        private const string LOCKED_IMAGES = "Wall_P1,Wall_P2,Wall_P3,Wall_P4,Wall_P5,";

//        public override void OnInitializeMelon()
//        {
//            _configPath = Path.Combine(Application.persistentDataPath, "gallery_unlock_config.json");
//            _imagesPath = Path.Combine(Application.persistentDataPath, "images.txt");
//            _imagesPersistentPath = Path.Combine(Application.persistentDataPath, "imagesPersistent.txt");

//            MelonLogger.Msg("GalleryUnlocker initialized!");
//            MelonLogger.Msg($"Images path: {_imagesPath}");
//            MelonLogger.Msg($"Comics: Toggle with '=' key");

//            try
//            {
//                var dialogueLuaType = AccessTools.TypeByName("DialogueLua");
//                if (dialogueLuaType != null)
//                {
//                    _dialogueLuaSetVariable = dialogueLuaType.GetMethod("SetVariable", new[] { typeof(string), typeof(string) });
//                    MelonLogger.Msg("DialogueLua ready!");
//                }
//            }
//            catch (Exception ex)
//            {
//                MelonLogger.Error($"Error finding DialogueLua: {ex.Message}");
//            }
//        }

//        public override void OnUpdate()
//        {
//            if (!_configLoaded)
//            {
//                LoadConfig();
//                _configLoaded = true;
//            }

//            bool isPressed = Keyboard.current.equalsKey.isPressed || Keyboard.current.numpadPlusKey.isPressed;

//            if (isPressed && !_wasPressed)
//            {
//                _isUnlocked = !_isUnlocked;
//                MelonLogger.Msg($"=== GalleryUnlock: {_isUnlocked} ===");

//                if (_isUnlocked)
//                    UnlockAll();
//                else
//                    LockAll();

//                SaveConfig();
//                MelonLogger.Msg("Restart game to apply changes!");
//            }
//            _wasPressed = isPressed;
//        }

//        private void SetVariable(string key, string value)
//        {
//            try
//            {
//                _dialogueLuaSetVariable?.Invoke(null, new object[] { key, value });
//            }
//            catch (Exception ex)
//            {
//                MelonLogger.Error($"Error setting {key}: {ex.Message}");
//            }
//        }

//        private void UnlockAll()
//        {
//            File.WriteAllText(_imagesPath, ALL_IMAGES);
//            File.WriteAllText(_imagesPersistentPath, ALL_IMAGES);

//            SetVariable("UnlockedCS", ALL_COMICS);
//            SetVariable("HasSpyAppLizGallery", "true");
//            SetVariable("HasSpyAppMattGallery", "true");

//            var images = ALL_IMAGES.Split(',', StringSplitOptions.RemoveEmptyEntries);
//            foreach (var img in images)
//            {
//                MelonLogger.Msg($"Unlocked image: {img}");
//            }

//            var comics = ALL_COMICS.Split(',', StringSplitOptions.RemoveEmptyEntries);
//            foreach (var comic in comics)
//            {
//                MelonLogger.Msg($"Unlocked comic: {comic}");
//            }

//            MelonLogger.Msg($"Total: {images.Length} images, {comics.Length} comics");
//        }

//        private void LockAll()
//        {
//            File.WriteAllText(_imagesPath, LOCKED_IMAGES);
//            File.WriteAllText(_imagesPersistentPath, "1,");

//            SetVariable("UnlockedCS", "");
//            SetVariable("HasSpyAppLizGallery", "false");
//            SetVariable("HasSpyAppMattGallery", "false");

//            MelonLogger.Msg("Locked all galleries!");
//        }

//        private void LoadConfig()
//        {
//            try
//            {
//                if (File.Exists(_configPath))
//                {
//                    var json = File.ReadAllText(_configPath);
//                    if (json.Contains("\"unlockAllGalleries\":true"))
//                    {
//                        _isUnlocked = true;
//                        UnlockAll();
//                        MelonLogger.Msg("Loaded config: Galleries unlocked!");
//                    }
//                }
//            }
//            catch (Exception ex)
//            {
//                MelonLogger.Error($"Error loading config: {ex.Message}");
//            }
//        }

//        private void SaveConfig()
//        {
//            try
//            {
//                var json = _isUnlocked
//                    ? "{\"unlockAllGalleries\":true}"
//                    : "{\"unlockAllGalleries\":false}";
//                File.WriteAllText(_configPath, json);
//            }
//            catch (Exception ex)
//            {
//                MelonLogger.Error($"Error saving config: {ex.Message}");
//            }
//        }
//    }
//}