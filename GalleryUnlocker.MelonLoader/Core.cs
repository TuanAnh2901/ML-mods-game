using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Linq;
using HarmonyLib;
using MelonLoader;
using UnityEngine;
using UnityEngine.InputSystem;

[assembly: MelonInfo(typeof(GalleryUnlocker.MelonLoader.Core), "GalleryUnlocker", "1.0.0", "User")]
[assembly: MelonGame("NosyFellow", "NTRMobile")]
[assembly: MelonPlatformDomain(MelonPlatformDomainAttribute.CompatibleDomains.IL2CPP)]

namespace GalleryUnlocker.MelonLoader
{
    public class Core : MelonMod
    {
        private static string _configPath;
        private static string _imagesPath;
        private static string _imagesPersistentPath;

        private bool _wasPressed = false;
        private bool _configLoaded = false;
        private static bool _initializationComplete = false;
        private static bool _init1Done = false;
        private static bool _init2Done = false;
        private static bool _init3Done = false;

        private static List<string> _capturedImages = new List<string>();
        private static List<string> _capturedComicNames = new List<string>();

        private static MethodInfo _dialogueLuaSetVariable;
        private static Type _comicDataManagerType;
        private static Type _comicEntryType;
        private static FieldInfo _comicEntriesField;
        private static MethodInfo _saveComicsDataMethod;
        private static object _comicDataManagerInstance;
        private static bool _typesSearched = false;

        public override void OnInitializeMelon()
        {
            var harmony = new HarmonyLib.Harmony("GalleryUnlocker");

            var imageGalleryType = AccessTools.TypeByName("ImageGallery");
            if (imageGalleryType != null)
            {
                try
                {
                    harmony.Patch(AccessTools.Method(imageGalleryType, "AddUnlockImageEntry", new[] { typeof(string), typeof(string), typeof(bool), typeof(bool), typeof(string) }),
                        prefix: new HarmonyMethod(typeof(ImageGalleryHooks).GetMethod("AddUnlockImageEntryPrefix")));
                    MelonLogger.Msg("Patched AddUnlockImageEntry");
                }
                catch (Exception ex) { MelonLogger.Error($"Error patching AddUnlockImageEntry: {ex.Message}"); }

                try
                {
                    harmony.Patch(AccessTools.Method(imageGalleryType, "AddUnlockComicEntry", new[] { typeof(string), typeof(string), typeof(string), typeof(bool), typeof(bool), typeof(int) }),
                        prefix: new HarmonyMethod(typeof(ImageGalleryHooks).GetMethod("AddUnlockComicEntryPrefix")));
                    MelonLogger.Msg("Patched AddUnlockComicEntry");
                }
                catch (Exception ex) { MelonLogger.Error($"Error patching AddUnlockComicEntry: {ex.Message}"); }

                try
                {
                    harmony.Patch(AccessTools.Method(imageGalleryType, "initializeImageGalleryVariables1"),
                        postfix: new HarmonyMethod(typeof(ImageGalleryHooks).GetMethod("InitPostfix1")));
                    harmony.Patch(AccessTools.Method(imageGalleryType, "initializeImageGalleryVariables2"),
                        postfix: new HarmonyMethod(typeof(ImageGalleryHooks).GetMethod("InitPostfix2")));
                    harmony.Patch(AccessTools.Method(imageGalleryType, "initializeImageGalleryVariables3"),
                        postfix: new HarmonyMethod(typeof(ImageGalleryHooks).GetMethod("InitPostfix3")));
                    MelonLogger.Msg("Patched init methods");
                }
                catch (Exception ex) { MelonLogger.Error($"Error patching init methods: {ex.Message}"); }
            }

            var comicViewControllerType = AccessTools.TypeByName("ComicViewController");
            if (comicViewControllerType != null)
            {
                try
                {
                    harmony.Patch(AccessTools.Method(comicViewControllerType, "ShowComicViewChat"),
                        prefix: new HarmonyMethod(typeof(ImageGalleryHooks).GetMethod("ShowComicViewChatPrefix")));
                    MelonLogger.Msg("Patched ShowComicViewChat");
                }
                catch (Exception ex) { MelonLogger.Error($"Error patching ShowComicViewChat: {ex.Message}"); }
            }

            _configPath = Path.Combine(Application.persistentDataPath, "gallery_unlock_config.json");
            _imagesPath = Path.Combine(Application.persistentDataPath, "images.txt");
            _imagesPersistentPath = Path.Combine(Application.persistentDataPath, "imagesPersistent.txt");

            try
            {
                var dialogueLuaType = AccessTools.TypeByName("DialogueLua");
                if (dialogueLuaType != null)
                {
                    _dialogueLuaSetVariable = dialogueLuaType.GetMethod("SetVariable", new[] { typeof(string), typeof(string) });
                    MelonLogger.Msg("DialogueLua ready!");
                }
            }
            catch (Exception ex)
            {
                MelonLogger.Error($"Error initializing DialogueLua: {ex.Message}");
            }

            MelonLogger.Msg("GalleryUnlocker initialized!");
            MelonLogger.Msg($"Images path: {_imagesPath}");
            MelonLogger.Msg("Press '=' to toggle unlock");
        }

        public override void OnUpdate()
        {
            if (!_configLoaded)
            {
                LoadConfig();
                _configLoaded = true;
            }

            if (!_typesSearched)
            {
                SearchComicTypes();
                _typesSearched = true;
            }

            if (Keyboard.current == null) return;

            bool isPressed = Keyboard.current.equalsKey.isPressed || Keyboard.current.numpadPlusKey.isPressed;

            if (isPressed && !_wasPressed)
            {
                if (!_initializationComplete)
                {
                    MelonLogger.Warning("Gallery not initialized yet. Please wait for game to load...");
                }
                else
                {
                    UnlockAllComics();
                    SaveConfig();
                }
            }
            _wasPressed = isPressed;
        }

        private void SearchComicTypes()
        {
            _comicDataManagerType = AccessTools.TypeByName("ComicDataManager");
            _comicEntryType = AccessTools.TypeByName("ComicEntry");

            if (_comicDataManagerType != null)
            {
                MelonLogger.Msg($"Found ComicDataManager: {_comicDataManagerType.FullName}");
                _comicEntriesField = AccessTools.Field(_comicDataManagerType, "comicEntries");
                _saveComicsDataMethod = AccessTools.Method(_comicDataManagerType, "SaveComicsData");
                MelonLogger.Msg($"Fields: entries={(_comicEntriesField != null)}, saveMethod={(_saveComicsDataMethod != null)}");
            }
            else
            {
                MelonLogger.Error("ComicDataManager type not found!");
            }
        }

        public static void TryCaptureComicDataManagerInstance(object instanceWithComicDataManager)
        {
            if (_comicDataManagerInstance != null) return;
            if (instanceWithComicDataManager == null) return;
            if (_comicDataManagerType == null) return;

            try
            {
                var field = instanceWithComicDataManager.GetType().GetField("comicDataManager");
                if (field != null)
                {
                    _comicDataManagerInstance = field.GetValue(instanceWithComicDataManager);
                    if (_comicDataManagerInstance != null)
                    {
                        MelonLogger.Msg("Got ComicDataManager instance!");
                    }
                }
            }
            catch (Exception ex)
            {
                MelonLogger.Error($"Error capturing ComicDataManager: {ex.Message}");
            }
        }

        private void SetVariable(string key, string value)
        {
            try
            {
                _dialogueLuaSetVariable?.Invoke(null, new object[] { key, value });
            }
            catch (Exception ex)
            {
                MelonLogger.Error($"Error setting {key}: {ex.Message}");
            }
        }

        private void UnlockAllComics()
        {
            MelonLogger.Msg("=== UNLOCK ALL triggered ===");

            // Comics - skip for now
            //if (_comicDataManagerInstance != null && _comicEntriesField != null)
            //{
            //    try
            //    {
            //        var comicEntries = _comicEntriesField.GetValue(_comicDataManagerInstance) as IList<object>;
            //        if (comicEntries != null)
            //        {
            //            int addedCount = 0;
            //            foreach (string comicName in _capturedComicNames)
            //            {
            //                bool exists = false;
            //                foreach (object entry in comicEntries)
            //                {
            //                    var nameField = entry.GetType().GetField("comicName");
            //                    if (nameField != null)
            //                    {
            //                        string name = nameField.GetValue(entry) as string;
            //                        if (name == comicName)
            //                        {
            //                            exists = true;
            //                            break;
            //                        }
            //                    }
            //                }
            //
            //                if (!exists)
            //                {
            //                    var newEntry = Activator.CreateInstance(_comicEntryType, new object[] { comicName });
            //                    comicEntries.Add(newEntry);
            //                    addedCount++;
            //                    MelonLogger.Msg($"[ADDED COMIC] {comicName}");
            //                }
            //            }
            //
            //            if (addedCount > 0 && _saveComicsDataMethod != null)
            //            {
            //                _saveComicsDataMethod.Invoke(_comicDataManagerInstance, null);
            //                MelonLogger.Msg($"[SAVED] {addedCount} comics");
            //            }
            //        }
            //        else
            //        {
            //            MelonLogger.Error("comicEntries is null!");
            //        }
            //    }
            //    catch (Exception ex)
            //    {
            //        MelonLogger.Error($"Error unlocking comics: {ex.Message}");
            //    }
            //}
            //else
            //{
            //    string allComicNames = string.Join(",", _capturedComicNames);
            //    SetVariable("UnlockedCS", allComicNames);
            //    MelonLogger.Msg($"Set UnlockedCS = {allComicNames}");
            //}

            string allImages = string.Join(",", _capturedImages) + ",";
            File.WriteAllText(_imagesPath, allImages);
            File.WriteAllText(_imagesPersistentPath, allImages);
            MelonLogger.Msg($"[UNLOCKED] {_capturedImages.Count} images");

            SetVariable("HasSpyAppLizGallery", "true");
            SetVariable("HasSpyAppMattGallery", "true");

            MelonLogger.Msg($"=== UNLOCK COMPLETE: {_capturedImages.Count} images, {_capturedComicNames.Count} comics (comics disabled) ===");
            MelonLogger.Msg("Restart game to apply changes!");
        }

        private void LoadConfig()
        {
            try
            {
                if (File.Exists(_configPath))
                {
                    var json = File.ReadAllText(_configPath);
                    if (json.Contains("\"unlockAllGalleries\":true"))
                    {
                        MelonLogger.Msg("Config: Auto-unlock enabled. Press '=' to activate!");
                    }
                }
            }
            catch (Exception ex)
            {
                MelonLogger.Error($"Error loading config: {ex.Message}");
            }
        }

        private void SaveConfig()
        {
            try
            {
                var json = "{\"unlockAllGalleries\":true}";
                File.WriteAllText(_configPath, json);
            }
            catch (Exception ex)
            {
                MelonLogger.Error($"Error saving config: {ex.Message}");
            }
        }

        public static void CaptureImage(string imageName)
        {
            if (!string.IsNullOrEmpty(imageName) && !_capturedImages.Contains(imageName))
            {
                _capturedImages.Add(imageName);
                MelonLogger.Msg($"[IMAGE CAPTURED] {imageName} (total: {_capturedImages.Count})");
            }
        }

        public static void CaptureComic(string comicName)
        {
            if (!string.IsNullOrEmpty(comicName) && !_capturedComicNames.Contains(comicName))
            {
                _capturedComicNames.Add(comicName);
                MelonLogger.Msg($"[COMIC CAPTURED] {comicName} (total: {_capturedComicNames.Count})");
            }
        }

        public static void SetInitializationComplete()
        {
            if (_init1Done && _init2Done && _init3Done)
            {
                _initializationComplete = true;
                MelonLogger.Msg($"=== Initialization Complete: {_capturedImages.Count} images, {_capturedComicNames.Count} comics captured ===");
            }
        }

        public static void SetInit1Done() { _init1Done = true; SetInitializationComplete(); }
        public static void SetInit2Done() { _init2Done = true; SetInitializationComplete(); }
        public static void SetInit3Done() { _init3Done = true; SetInitializationComplete(); }
    }

    public static class ImageGalleryHooks
    {
        public static void AddUnlockImageEntryPrefix(string id, string imageName, bool unlocked, bool hasAnimation, string imageText)
        {
            Core.CaptureImage(imageName);
        }

        public static void AddUnlockComicEntryPrefix(string id, string previewImage, string comicName, bool unlocked, bool hasChoice, int choicesCount, object __instance = null)
        {
            Core.CaptureComic(comicName);
            //Core.TryCaptureComicDataManagerInstance(__instance);
        }

        public static void InitPostfix1(object __instance = null)
        {
            MelonLogger.Msg("[INIT1 DONE]");
            if (__instance != null)
            {
                Core.TryCaptureComicDataManagerInstance(__instance);
            }
            Core.SetInit1Done();
        }

        public static void InitPostfix2(object __instance = null)
        {
            MelonLogger.Msg("[INIT2 DONE]");
            if (__instance != null)
            {
                Core.TryCaptureComicDataManagerInstance(__instance);
            }
            Core.SetInit2Done();
        }

        public static void InitPostfix3(object __instance = null)
        {
            MelonLogger.Msg("[INIT3 DONE]");
            if (__instance != null)
            {
                Core.TryCaptureComicDataManagerInstance(__instance);
            }
            Core.SetInit3Done();
        }

        public static void ShowComicViewChatPrefix(object __instance = null)
        {
            if (__instance != null)
            {
                Core.TryCaptureComicDataManagerInstance(__instance);
            }
        }
    }
}