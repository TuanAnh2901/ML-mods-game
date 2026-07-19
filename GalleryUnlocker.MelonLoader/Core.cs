using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using HarmonyLib;
using MelonLoader;
using UnityEngine;
using UnityEngine.InputSystem;

[assembly: MelonInfo(typeof(GalleryUnlocker.MelonLoader.Core), "GalleryUnlocker", "1.1.0", "User")]
[assembly: MelonGame("NosyFellow", "NTRMobile")]
[assembly: MelonPlatformDomain(MelonPlatformDomainAttribute.CompatibleDomains.IL2CPP)]

namespace GalleryUnlocker.MelonLoader
{
    public class Core : MelonMod
    {
        private static string _configPath;
        private static string _imagesPath;
        private static string _imagesPersistentPath;
        private static string _comicsPersistentPath;

        private bool _wasPressed = false;
        private bool _configLoaded = false;

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
            var imageGalleryType = AccessTools.TypeByName("NTRMobile.ImageGallery")
                ?? AccessTools.TypeByName("ImageGallery");
            MelonLogger.Msg(imageGalleryType == null
                ? "ImageGallery type not found during startup; retrying through reflection on key press."
                : $"ImageGallery type found: {imageGalleryType.FullName}");

            _configPath = Path.Combine(Application.persistentDataPath, "gallery_unlock_config.json");
            _imagesPath = Path.Combine(Application.persistentDataPath, "images.txt");
            _imagesPersistentPath = Path.Combine(Application.persistentDataPath, "imagesPersistent.txt");
            _comicsPersistentPath = Path.Combine(Application.persistentDataPath, "comicsPersistent.txt");

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
                UnlockAllComics();
                SaveConfig();
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
            MelonLogger.Msg("=== UNLOCK ALL GALLERIES triggered ===");

            var imageGalleryType = AccessTools.TypeByName("NTRMobile.ImageGallery")
                ?? AccessTools.TypeByName("ImageGallery");
            var instanceProperty = imageGalleryType == null
                ? null
                : AccessTools.Property(imageGalleryType, "Instance");
            var gallery = instanceProperty?.GetValue(null);

            if (gallery == null)
            {
                MelonLogger.Warning("ImageGallery.Instance is not ready yet.");
                return;
            }

            var unlockedImages = new HashSet<string>(StringComparer.Ordinal);
            foreach (var fieldName in new[] { "_imageLookup", "_videoLookup" })
            {
                foreach (var name in ReadLookupKeys(gallery, imageGalleryType, fieldName))
                {
                    if (unlockedImages.Add(name))
                    {
                        InvokeStringMethod(gallery, "AddToDefaultGallery", name);
                        InvokeStringMethod(gallery, "AddToPersistentGallery", name);
                    }
                }
            }

            var unlockedComics = new HashSet<string>(StringComparer.Ordinal);
            var unlockedComicIds = new HashSet<string>(StringComparer.Ordinal);
            foreach (var entry in ReadLookupEntries(gallery, imageGalleryType, "_comicLookup"))
            {
                var name = entry.Key;
                if (unlockedComics.Add(name))
                {
                    InvokeStringMethod(gallery, "UnlockComicGalleryImages", name);
                }

                // The gallery lookup key is the internal comicName (for example
                // Liz_CS_5).  The tile counter is driven by GameManager.UnlockedCS
                // and expects ComicEntry.id values (LC1, RC1, ...), not lookup keys.
                var id = ReadStringMember(entry.Value, "id");
                if (!string.IsNullOrWhiteSpace(id)) unlockedComicIds.Add(id);
            }

            // These are the native ImageGallery methods recovered from the 0.24 dump.
            InvokeNoArgMethod(gallery, "RebuildPersistentNavList");
            InvokeNoArgMethod(gallery, "RefreshGallery");

            var unlockedCs = string.Join(",", unlockedComicIds.OrderBy(x => x, StringComparer.Ordinal));
            SetVariable("UnlockedCS", unlockedCs);
            SetVariable("App_HasSpyAppLizGallery", "true");
            SetVariable("App_HasSpyAppMattGallery", "true");

            var gameManagerType = AccessTools.TypeByName("Il2CppNTRMobile.GameManager")
                ?? AccessTools.TypeByName("NTRMobile.GameManager")
                ?? AccessTools.TypeByName("GameManager");
            var gameManager = GetStaticPropertyValue(gameManagerType, "Instance");
            var propertyUpdated = SetInstanceProperty(gameManager, "UnlockedCS", unlockedCs);

            // Keep the Lua/save-variable cache in sync before SaveSystem
            // snapshots the current slot. Re-apply the property afterwards
            // because this routine can rebuild cached values from Lua.
            InvokeNoArgMethod(gameManager, "UpdateSaveVariables");
            propertyUpdated = SetInstanceProperty(gameManager, "UnlockedCS", unlockedCs) || propertyUpdated;

            // SaveSystem.Save() serializes the active GameManager state to
            // AppData. Calling it is necessary; changing the Lua table alone
            // does not update saveData.txt.
            // TypeByName("SaveSystem") can resolve PixelCrushers.SaveSystem
            // first.  The game implementation is the generated Il2Cpp proxy
            // in the Il2CppNTRMobile namespace, so resolve that exact type
            // before using compatibility fallbacks.
            var saveSystemType = AccessTools.TypeByName("Il2CppNTRMobile.SaveSystem")
                ?? AccessTools.TypeByName("NTRMobile.SaveSystem");
            var saveSystem = GetStaticPropertyValue(saveSystemType, "Instance");
            var saved = InvokeNoArgMethod(saveSystem, "Save");
            var filePatched = PatchActiveSaveData(unlockedCs);
            var comicsFilePatched = WriteComicsPersistent(unlockedComics);
            // Rebuild the visible comic list after GameManager.UnlockedCS has
            // changed; the earlier refresh happened before the save state was
            // updated and only refreshed the old 6/81 view.
            InvokeNoArgMethod(gallery, "RefreshGallery");

            MelonLogger.Msg($"[UNLOCKED] {unlockedImages.Count} images/videos, {unlockedComics.Count} comics, {unlockedComicIds.Count} comic IDs");
            MelonLogger.Msg($"[SAVE] UnlockedCS={unlockedComicIds.Count}, GameManager property={(propertyUpdated ? "updated" : "not found")}, SaveSystem.Save={(saved ? "ok" : "not found")}, saveData.txt={(filePatched ? "patched" : "not patched")}, comicsPersistent.txt={(comicsFilePatched ? "written" : "not written")}");

            MelonLogger.Msg("=== UNLOCK COMPLETE: native ImageGallery state updated and persisted ===");
        }

        private static IEnumerable<string> ReadLookupKeys(object gallery, Type galleryType, string fieldName)
        {
            foreach (var entry in ReadLookupEntries(gallery, galleryType, fieldName))
                yield return entry.Key;
        }

        private static IEnumerable<KeyValuePair<string, object>> ReadLookupEntries(object gallery, Type galleryType, string fieldName)
        {
            var field = AccessTools.Field(galleryType, fieldName);
            var property = AccessTools.Property(galleryType, fieldName);
            var lookup = field?.GetValue(gallery) ?? property?.GetValue(gallery);
            if (lookup == null) yield break;

            var getEnumerator = lookup.GetType().GetMethod("GetEnumerator", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            var enumerator = getEnumerator?.Invoke(lookup, null);
            if (enumerator == null) yield break;

            var moveNext = enumerator.GetType().GetMethod("MoveNext", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            var currentProperty = enumerator.GetType().GetProperty("Current");
            var currentField = enumerator.GetType().GetField("Current");
            if (moveNext == null) yield break;

            while (Convert.ToBoolean(moveNext.Invoke(enumerator, null)))
            {
                var entry = currentProperty?.GetValue(enumerator) ?? currentField?.GetValue(enumerator);
                if (entry == null) continue;
                var entryType = entry.GetType();
                var keyProperty = entryType.GetProperty("Key");
                var keyField = entryType.GetField("Key");
                var valueProperty = entryType.GetProperty("Value");
                var valueField = entryType.GetField("Value");
                var key = keyProperty?.GetValue(entry) ?? keyField?.GetValue(entry);
                var value = valueProperty?.GetValue(entry) ?? valueField?.GetValue(entry);
                if (key is string name && !string.IsNullOrWhiteSpace(name))
                    yield return new KeyValuePair<string, object>(name, value);
            }
        }

        private static string ReadStringMember(object target, string memberName)
        {
            if (target == null) return null;
            var type = target.GetType();
            var property = AccessTools.Property(type, memberName);
            var field = AccessTools.Field(type, memberName);
            var value = property?.GetValue(target) ?? field?.GetValue(target);
            return value as string;
        }

        private static object GetStaticPropertyValue(Type type, string propertyName)
        {
            if (type == null) return null;
            try { return AccessTools.Property(type, propertyName)?.GetValue(null); }
            catch (Exception ex)
            {
                MelonLogger.Warning($"{type.FullName}.{propertyName} read failed: {ex.GetBaseException().Message}");
                return null;
            }
        }

        private static bool SetInstanceProperty(object target, string propertyName, object value)
        {
            if (target == null) return false;
            try
            {
                var property = AccessTools.Property(target.GetType(), propertyName);
                if (property?.CanWrite != true) return false;
                property.SetValue(target, value);
                return true;
            }
            catch (Exception ex)
            {
                MelonLogger.Warning($"{propertyName} update failed: {ex.GetBaseException().Message}");
                return false;
            }
        }

        private static bool PatchActiveSaveData(string unlockedCs)
        {
            try
            {
                var savePath = Path.Combine(Application.persistentDataPath, "saveData.txt");
                if (!File.Exists(savePath)) return false;

                var text = File.ReadAllText(savePath);
                var replacement = "UnlockedCS=\"" + unlockedCs.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
                var updated = System.Text.RegularExpressions.Regex.Replace(
                    text, "UnlockedCS=\\\"[^\\\"]*\\\"", replacement);
                if (updated == text) return false;

                var backup = savePath + ".galleryunlocker.bak";
                if (!File.Exists(backup)) File.Copy(savePath, backup);
                File.WriteAllText(savePath, updated);
                return true;
            }
            catch (Exception ex)
            {
                MelonLogger.Warning($"saveData.txt patch failed: {ex.GetBaseException().Message}");
                return false;
            }
        }

        private static bool WriteComicsPersistent(IEnumerable<string> comicNames)
        {
            try
            {
                var names = comicNames
                    .Where(x => !string.IsNullOrWhiteSpace(x))
                    .Distinct(StringComparer.Ordinal)
                    .OrderBy(x => x, StringComparer.Ordinal)
                    .ToList();
                if (names.Count == 0) return false;

                var choices = new Dictionary<string, bool[]>(StringComparer.Ordinal);
                if (File.Exists(_comicsPersistentPath))
                {
                    var existing = File.ReadAllText(_comicsPersistentPath);
                    foreach (Match match in System.Text.RegularExpressions.Regex.Matches(existing, "\\{(?<body>.*?)\\}", System.Text.RegularExpressions.RegexOptions.Singleline))
                    {
                        var body = match.Groups["body"].Value;
                        var nameMatch = System.Text.RegularExpressions.Regex.Match(body, "\\\"comicName\\\"\\s*:\\s*\\\"(?<name>(?:\\\\.|[^\\\"])*)\\\"");
                        if (!nameMatch.Success) continue;
                        var name = nameMatch.Groups["name"].Value;
                        choices[name] = new[]
                        {
                            RegexBool(body, "choice1"),
                            RegexBool(body, "choice2"),
                            RegexBool(body, "choice3")
                        };
                    }
                }

                var json = new StringBuilder("[\n");
                for (var i = 0; i < names.Count; i++)
                {
                    choices.TryGetValue(names[i], out var state);
                    state ??= new[] { false, false, false };
                    json.Append("        {\n");
                    json.Append("            \"comicName\": \"").Append(JsonEscape(names[i])).Append("\",\n");
                    json.Append("            \"choice1\": ").Append(state[0] ? "true" : "false").Append(",\n");
                    json.Append("            \"choice2\": ").Append(state[1] ? "true" : "false").Append(",\n");
                    json.Append("            \"choice3\": ").Append(state[2] ? "true" : "false").Append("\n");
                    json.Append("        }").Append(i + 1 == names.Count ? "\n" : ",\n");
                }
                json.Append("    ]\n");

                var backup = _comicsPersistentPath + ".galleryunlocker.bak";
                if (File.Exists(_comicsPersistentPath) && !File.Exists(backup)) File.Copy(_comicsPersistentPath, backup);
                File.WriteAllText(_comicsPersistentPath, json.ToString());
                return true;
            }
            catch (Exception ex)
            {
                MelonLogger.Warning($"comicsPersistent.txt write failed: {ex.GetBaseException().Message}");
                return false;
            }
        }

        private static bool RegexBool(string body, string key)
        {
            return bool.TryParse(System.Text.RegularExpressions.Regex.Match(body, $"\\\"{key}\\\"\\s*:\\s*(?<value>true|false)").Groups["value"].Value, out var value) && value;
        }

        private static string JsonEscape(string value)
        {
            return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        private static bool InvokeStringMethod(object target, string methodName, string value)
        {
            var method = target.GetType().GetMethods(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)
                .FirstOrDefault(m => m.Name == methodName && m.GetParameters().Length == 1 &&
                                     m.GetParameters()[0].ParameterType == typeof(string));
            if (method == null) return false;
            try
            {
                method.Invoke(target, new object[] { value });
                return true;
            }
            catch (Exception ex)
            {
                MelonLogger.Warning($"{methodName}({value}) failed: {ex.GetBaseException().Message}");
                return false;
            }
        }

        private static bool InvokeNoArgMethod(object target, string methodName)
        {
            if (target == null) return false;
            var method = target.GetType().GetMethods(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic)
                .FirstOrDefault(m => m.Name == methodName && m.GetParameters().Length == 0);
            if (method == null) return false;
            try
            {
                method.Invoke(target, null);
                return true;
            }
            catch (Exception ex)
            {
                MelonLogger.Warning($"{methodName}() failed: {ex.GetBaseException().Message}");
                return false;
            }
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

    }
}
