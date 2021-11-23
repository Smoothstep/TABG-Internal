using System;
using UnityEngine;
using System.Diagnostics;
using Landfall.Network;
using System.Runtime.InteropServices;
using System.Reflection;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using Microsoft.Win32.SafeHandles;
using System.Text;

namespace TABG_SampleModule
{
    public class Sample
    {
        public class PlayerStatsMod : MonoBehaviour
        {
            void Start()
            {
                InvokeRepeating(nameof(ChangeStats), 1.0f, 0.1f);
            }

            void ChangeStats()
            {
                try
                {
                    var player = Player.localPlayer;
                    if (!player)
                    {
                        return;
                    }

                    Player.localPlayer.stats.extraJumps = 1000;
                    Player.localPlayer.stats.speedMultiplier = 3f;
                    Player.localPlayer.stats.movementSpeedAdd = 2f;
                    Player.localPlayer.stats.bulletSpeedMultiplier = 20f;
                    Player.localPlayer.stats.selfKnockbackMultiplier = 0f;
                    Player.localPlayer.stats.bulletDmgMultiplier = 5f;
                    Player.localPlayer.stats.lifeStealAdd = 100f;
                    Player.localPlayer.stats.healingMultipier = 1000f;
                    Player.localPlayer.stats.healthMultiplier = 5f;
                    Player.localPlayer.stats.spreadMultiplier = 0f;
                    Player.localPlayer.stats.recoilMultiplier = 0f;
                }
                catch (Exception ex)
                {
                    Console.WriteLine(ex.Message);
                }
            }
        }

        private static GameObject Startup;

        public static void Load()
        {
            Startup = new GameObject();
            Startup.AddComponent<PlayerStatsMod>();
            UnityEngine.Object.DontDestroyOnLoad(Startup);
        }
    }
}
