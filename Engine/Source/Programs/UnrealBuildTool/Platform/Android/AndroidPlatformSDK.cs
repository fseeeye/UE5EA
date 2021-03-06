// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using EpicGames.Core;
using System.Text.RegularExpressions;
using System.Linq;

#nullable disable

namespace UnrealBuildTool
{
	class AndroidPlatformSDK : UEBuildPlatformSDK
	{
		public override string GetMainVersion()
		{
			return "r21b";
		}
		public override string GetAutoSDKDirectoryForMainVersion()
		{
			return "-23";
		}

		public override void GetValidVersionRange(out string MinVersion, out string MaxVersion)
		{
			MinVersion = "r21a";
			MaxVersion = "r23a";
		}

		public override void GetValidSoftwareVersionRange(out string MinVersion, out string MaxVersion)
		{
			MinVersion = MaxVersion = null;
		}

		public override string GetPlatformSpecificVersion(string VersionType)
		{
			switch (VersionType.ToLower())
			{
				case "platforms": return "android-28";
				case "build-tools": return "28.0.3";
				case "cmake": return "3.10.2.4988404";
				case "ndk": return "21.4.7075529";
			}

			return "";
		}


		public override string GetInstalledSDKVersion()
		{
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");

			// don't register if we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				return null;
			}

			NDKPath = NDKPath.Replace("\"", "");

			// figure out the NDK version
			string NDKToolchainVersion = null;
			string SourcePropFilename = Path.Combine(NDKPath, "source.properties");
			if (File.Exists(SourcePropFilename))
			{
				string RevisionString = "";
				string[] PropertyContents = File.ReadAllLines(SourcePropFilename);
				foreach (string PropertyLine in PropertyContents)
				{
					if (PropertyLine.StartsWith("Pkg.Revision"))
					{
						RevisionString = PropertyLine;
						break;
					}
				}

				int EqualsIndex = RevisionString.IndexOf('=');
				if (EqualsIndex > 0)
				{
					string[] RevisionParts = RevisionString.Substring(EqualsIndex + 1).Trim().Split('.');
					int RevisionMinor = int.Parse(RevisionParts.Length > 1 ? RevisionParts[1] : "0");
					char RevisionLetter = Convert.ToChar('a' + RevisionMinor);
					NDKToolchainVersion = "r" + RevisionParts[0] + (RevisionMinor > 0 ? Char.ToString(RevisionLetter) : "");
				}
			}
			else
			{
				string ReleaseFilename = Path.Combine(NDKPath, "RELEASE.TXT");
				if (File.Exists(ReleaseFilename))
				{
					string[] PropertyContents = File.ReadAllLines(SourcePropFilename);
					NDKToolchainVersion = PropertyContents[0];
				}
			}

			// return something like r10e, or null if anything went wrong above
			return NDKToolchainVersion;

		}


		public override bool TryConvertVersionToInt(string StringValue, out UInt64 OutValue)
		{
			// convert r<num>[letter] to hex
			if (!string.IsNullOrEmpty(StringValue))
			{
				Match Result = Regex.Match(StringValue, @"^r(\d*)([a-z])?");

				if (Result.Success)
				{
					// 8 for number, 8 for letter, 8 for unused patch
					int RevisionNumber = 0;
					if (Result.Groups[2].Success)
					{
						RevisionNumber = (Result.Groups[2].Value[0] - 'a') + 1;
					}
					string VersionString = string.Format("{0}{1:00}{2:00}", Result.Groups[1], RevisionNumber, 0);
					return UInt64.TryParse(VersionString, out OutValue);
				}
			}

			OutValue = 0;
			return false;
		}


		public override SDKStatus PrintSDKInfoAndReturnValidity(LogEventType Verbosity, LogFormatOptions Options, LogEventType ErrorVerbosity, LogFormatOptions ErrorOptions)
		{
			SDKStatus Validity = base.PrintSDKInfoAndReturnValidity(Verbosity, Options, ErrorVerbosity, ErrorOptions);

			if (GetInstalledVersion() != GetMainVersion())
			{
				Log.WriteLine(Verbosity, Options, "Note: Android toolchain NDK {0} recommended", GetMainVersion());
			}

			return Validity;
		}


		protected override bool PlatformSupportsAutoSDKs()
		{
			return true;
		}

		protected override String GetRequiredScriptVersionString()
		{
			return "3.6";
		}

		// prefer auto sdk on android as correct 'manual' sdk detection isn't great at the moment.
		protected override bool PreferAutoSDK()
		{
			return true;
		}

		private static bool ExtractPath(string Source, out string Path)
		{
			int start = Source.IndexOf('"');
			int end = Source.LastIndexOf('"');
			if (start != 1 && end != -1 && start < end)
			{
				++start;
				Path = Source.Substring(start, end - start);
				return true;
			}
			else
			{
				Path = "";
			}

			return false;
		}

		public static bool GetPath(ConfigHierarchy Ini, string SectionName, string Key, out string Value)
		{
			string temp;
			if (Ini.TryGetValue(SectionName, Key, out temp))
			{
				return ExtractPath(temp, out Value);
			}
			else
			{
				Value = "";
			}

			return false;
		}

		/// <summary>
		/// checks if the sdk is installed or has been synced
		/// </summary>
		/// <returns></returns>
		protected virtual bool HasAnySDK()
		{
			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT");
			{
				ConfigHierarchy configCacheIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, (DirectoryReference)null, BuildHostPlatform.Current.Platform);
				Dictionary<string, string> AndroidEnv = new Dictionary<string, string>();

				Dictionary<string, string> EnvVarNames = new Dictionary<string, string> {
														 {"ANDROID_HOME", "SDKPath"},
														 {"NDKROOT", "NDKPath"},
														 {"JAVA_HOME", "JavaPath"}
														 };

				string path;
				foreach (KeyValuePair<string, string> kvp in EnvVarNames)
				{
					if (GetPath(configCacheIni, "/Script/AndroidPlatformEditor.AndroidSDKSettings", kvp.Value, out path) && !string.IsNullOrEmpty(path))
					{
						AndroidEnv.Add(kvp.Key, path);
					}
					else
					{
						string envValue = Environment.GetEnvironmentVariable(kvp.Key);
						if (!String.IsNullOrEmpty(envValue))
						{
							AndroidEnv.Add(kvp.Key, envValue);
						}
					}
				}

				// If we are on Mono and we are still missing a key then go and find it from the .bash_profile
				if (Utils.IsRunningOnMono && !EnvVarNames.All(s => AndroidEnv.ContainsKey(s.Key)))
				{
					string BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bash_profile");
					if (!File.Exists(BashProfilePath))
					{
						// Try .bashrc if didn't fine .bash_profile
						BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bashrc");
					}
					if (File.Exists(BashProfilePath))
					{
						string[] BashProfileContents = File.ReadAllLines(BashProfilePath);

						// Walk backwards so we keep the last export setting instead of the first
						for (int LineIndex = BashProfileContents.Length - 1; LineIndex >= 0; --LineIndex)
						{
							foreach (KeyValuePair<string, string> kvp in EnvVarNames)
							{
								if (AndroidEnv.ContainsKey(kvp.Key))
								{
									continue;
								}

								if (BashProfileContents[LineIndex].StartsWith("export " + kvp.Key + "="))
								{
									string PathVar = BashProfileContents[LineIndex].Split('=')[1].Replace("\"", "");
									AndroidEnv.Add(kvp.Key, PathVar);
								}
							}
						}
					}
				}

				// Set for the process
				foreach (KeyValuePair<string, string> kvp in AndroidEnv)
				{
					Environment.SetEnvironmentVariable(kvp.Key, kvp.Value);
				}

				// See if we have an NDK path now...
				AndroidEnv.TryGetValue("NDKROOT", out NDKPath);
			}

			// we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				return false;
			}

			NDKPath = NDKPath.Replace("\"", "");

			// need a supported llvm
			if (!Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm")))
			{
				return false;
			}
			return true;
		}

		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			// if any autosdk setup has been done then the local process environment is suspect
			if (HasSetupAutoSDK())
			{
				return SDKStatus.Invalid;
			}

			if (HasAnySDK())
			{
				return SDKStatus.Valid;
			}

			return SDKStatus.Invalid;
		}
	}
}
