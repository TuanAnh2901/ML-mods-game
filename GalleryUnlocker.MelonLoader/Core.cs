using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
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
        private static List<string> _capturedComics = new List<string>();

        private static MethodInfo _dialogueLuaSetVariable;

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

            _configPath = Path.Combine(Application.persistentDataPath, "gallery_unlock_config.json");
            _imagesPath = Path.Combine(Application.persistentDataPath, "images.txt");
            _imagesPersistentPath = Path.Combine(Application.persistentDataPath, "imagesPersistent.txt");

            MelonLogger.Msg("GalleryUnlocker initialized!");
            MelonLogger.Msg($"Images path: {_imagesPath}");
            MelonLogger.Msg("Press '=' to toggle unlock");

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
                MelonLogger.Error($"Error initializing: {ex.Message}");
            }
        }

        public override void OnUpdate()
        {
            if (!_configLoaded)
            {
                LoadConfig();
                _configLoaded = true;
            }

            bool isPressed = Keyboard.current.equalsKey.isPressed || Keyboard.current.numpadPlusKey.isPressed;

            if (isPressed && !_wasPressed)
            {
                if (!_initializationComplete)
                {
                    MelonLogger.Warning("Gallery not initialized yet. Please wait for game to load...");
                }
                else
                {
                    UnlockAll();
                    SaveConfig();
                    MelonLogger.Msg("Restart game to apply changes!");
                }
            }
            _wasPressed = isPressed;
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

        private void UnlockAll()
        {
            string allImages = string.Join(",", _capturedImages) + ",";
            string allComics = string.Join(",", _capturedComics) + ",";

            File.WriteAllText(_imagesPath, allImages);
            File.WriteAllText(_imagesPersistentPath, allImages);

            SetVariable("UnlockedCS", allComics);
            SetVariable("HasSpyAppLizGallery", "true");
            SetVariable("HasSpyAppMattGallery", "true");

            MelonLogger.Msg($"Unlocked {_capturedImages.Count} images and {_capturedComics.Count} comics!");
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
            if (!string.IsNullOrEmpty(imageName))
            {
                _capturedImages.Add(imageName);
                MelonLogger.Msg($"[IMAGE CAPTURED] {imageName} (total: {_capturedImages.Count})");
            }
        }

        public static void CaptureComic(string comicName)
        {
            if (!string.IsNullOrEmpty(comicName))
            {
                _capturedComics.Add(comicName);
                MelonLogger.Msg($"[COMIC CAPTURED] {comicName} (total: {_capturedComics.Count})");
            }
        }

        public static void SetInitializationComplete()
        {
            if (!_initializationComplete && _capturedImages.Count > 0)
            {
                _initializationComplete = true;
                MelonLogger.Msg($"=== Initialization Complete: {_capturedImages.Count} images, {_capturedComics.Count} comics captured ===");

                string capturedListPath = Path.Combine(Application.persistentDataPath, "captured_images.txt");
                string allImages = string.Join(",", _capturedImages) + ",";
                File.WriteAllText(capturedListPath, allImages);

                string capturedComicsPath = Path.Combine(Application.persistentDataPath, "captured_comics.txt");
                string allComics = string.Join(",", _capturedComics);
                File.WriteAllText(capturedComicsPath, allComics);

                string allComicsDisplay = "[" + string.Join(",", _capturedComics.Select(c => $"\"{c}\"")) + "]";
                string displayPath = Path.Combine(Application.persistentDataPath, "captured_comics_display.txt");
                File.WriteAllText(displayPath, allComicsDisplay);

                MelonLogger.Msg($"Saved captured images to: {capturedListPath}");
                MelonLogger.Msg($"Saved captured comics to: {capturedComicsPath}");
                MelonLogger.Msg($"Comics display format: {allComicsDisplay}");
            }
        }

        public static void SetInit1Done() { _init1Done = true; CheckAllInitDone(); }
        public static void SetInit2Done() { _init2Done = true; CheckAllInitDone(); }
        public static void SetInit3Done() { _init3Done = true; CheckAllInitDone(); }

        private static void CheckAllInitDone()
        {
            if (_init1Done && _init2Done && _init3Done && _capturedImages.Count > 0)
            {
                SetInitializationComplete();
            }
        }
    }

    public static class ImageGalleryHooks
    {
        public static void AddUnlockImageEntryPrefix(string id, string imageName, bool unlocked, bool hasAnimation, string imageText)
        {
            Core.CaptureImage(imageName);
        }

        public static void AddUnlockComicEntryPrefix(string id, string previewImage, string comicName, bool unlocked, bool hasChoice, int choicesCount)
        {
            Core.CaptureComic(comicName);
        }

        public static void InitPostfix1()
        {
            Core.SetInit1Done();
        }

        public static void InitPostfix2()
        {
            Core.SetInit2Done();
        }

        public static void InitPostfix3()
        {
            Core.SetInit3Done();
        }
    }
}