using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace FemmyInjector
{
    class Program
    {
        // ==========================================
        // 🎀 KONFIGURASI APLIKASI
        // ==========================================
        static string AppPackage = "com.proximabeta.mf.uamo";
        static string AppActivity = "com.epicgames.ue4.GameActivity";
        static string UserId = "0";
        
        static string HistoryFile = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "femmy_history.txt");
        static bool isWaitingForKey = false;

        static void Main(string[] args)
        {
            Console.OutputEncoding = Encoding.UTF8;
            Console.InputEncoding = Encoding.UTF8;

            string fullPath = "";

            if (args.Length > 0)
            {
                fullPath = args[0].Replace("\"", "");
                File.WriteAllText(HistoryFile, fullPath);
            }
            else if (File.Exists(HistoryFile))
            {
                string savedPath = File.ReadAllText(HistoryFile).Trim();
                if (File.Exists(savedPath))
                {
                    Console.WriteLine("========================================================");
                    Console.WriteLine(" (◕‿◕✿) FEMMY INGET SCRIPT SENPAI SEBELUMNYA!");
                    Console.WriteLine($" Script: {savedPath}");
                    Console.WriteLine("========================================================\n");
                    
                    Console.Write("Mau pakai script ini lagi gak? [ENTER/Y = Ya, N = Ganti Baru]: ");
                    string choice = Console.ReadLine().Trim().ToUpper();
                    
                    if (choice == "Y" || choice == "") 
                    {
                        fullPath = savedPath;
                    }
                    else
                    {
                        File.Delete(HistoryFile); 
                    }
                }
            }

            if (string.IsNullOrEmpty(fullPath) || !File.Exists(fullPath))
            {
                Console.WriteLine("\n[INFO] Menunggu Script Baru...");
                Console.WriteLine("Silakan TUTUP jendela ini, lalu Tarik (Drag) file .sh BARU ke ikon .exe ya Senpai!");
                Console.ReadLine();
                return;
            }

            bool isRunning = true;
            while (isRunning)
            {
                RunInjector(fullPath);

                Console.WriteLine("\n========================================================");
                Console.WriteLine(" (づ｡◕‿‿◕｡)づ BERES SENPAI! HP UDAH BERSIH!");
                Console.WriteLine("========================================================");
                Console.Write("Mau RERUN (Main Lagi) script yang sama? [ENTER/Y = Ya, N = Keluar]: ");
                
                string rerunChoice = Console.ReadLine().Trim().ToUpper();
                if (rerunChoice == "N")
                {
                    isRunning = false;
                }
            }
        }

        static void RunInjector(string scriptPath)
        {
            isWaitingForKey = false; 
            
            string scriptFileName = Path.GetFileName(scriptPath);
            string targetPath = $"/data/local/tmp/{scriptFileName}";
            
            Console.Clear();
            Console.WriteLine("========================================================");
            Console.WriteLine(" (◕‿◕✿) FEMMY FLASHER - C# SMART EDITION (★^O^★)");
            Console.WriteLine("========================================================\n");
            
            Console.WriteLine($"[INFO] Target Script : {scriptFileName}");
            Console.WriteLine("[~] Nyari HP kamu nih, Senpai...");
            RunAdbCommand("wait-for-device");

            // 2. AUTO DETECT WORK PROFILE
            Console.WriteLine("[~] Melacak lokasi instalasi game secara otomatis...");
            string usersOutput = RunAdbCommand("shell pm list users");
            MatchCollection matches = Regex.Matches(usersOutput, @"\{(\d+):");
            
            bool found = false;
            foreach (Match match in matches)
            {
                string u = match.Groups[1].Value;
                int uInt = int.Parse(u);
                if (uInt >= 10) 
                {
                    string pathOutput = RunAdbCommand($"shell pm path --user {u} {AppPackage}");
                    if (pathOutput.Contains("package:"))
                    {
                        UserId = u;
                        found = true;
                        break;
                    }
                }
            }

            if (!found) Console.WriteLine($"[!] Gamenya ada di Ruang Utama (User 0).");
            else Console.WriteLine($"[~] BINGGO! Game ditemukan di Work Profile: User {UserId}");

            // 3. CLEAN UP, KILL TASKS & PUSH
            Console.WriteLine("[~] Matiin paksa game & nyapu cache...");
            RunAdbCommand($"shell am force-stop --user {UserId} {AppPackage}");
            RunAdbCommand($"shell su -c \"rm -rf /data/user/{UserId}/{AppPackage}/cache/*\"");

            // Pkill hanya dilakukan di AWAL untuk membersihkan proses lama
            Console.WriteLine($"[~] Membersihkan task lama '{scriptFileName}' yang masih nyangkut...");
            RunAdbCommand($"shell su -c \"pkill -f '{scriptFileName}'\"");

            Console.WriteLine("[~] Nyuntikin script baru ke HP...");
            RunAdbCommand($"push \"{scriptPath}\" \"{targetPath}\"");
            RunAdbCommand($"shell chmod 755 \"{targetPath}\"");

            Console.WriteLine("\n✿✿✿✿✿✿✿✿✿ MULAI INJECT ✿✿✿✿✿✿✿✿✿");

            Process adbShell = new Process();
            adbShell.StartInfo.FileName = "adb";
            adbShell.StartInfo.Arguments = $"shell su -c \"sh '{targetPath}'\"";
            adbShell.StartInfo.UseShellExecute = false;
            adbShell.StartInfo.RedirectStandardInput = true;
            adbShell.StartInfo.RedirectStandardOutput = true;
            adbShell.StartInfo.RedirectStandardError = true;
            adbShell.StartInfo.CreateNoWindow = true;
            adbShell.Start();

            // 5. MEMBACA OUTPUT & RADAR DETEKSI KATA
            Task.Run(() => {
                char[] buffer = new char[1024];
                int bytesRead;
                while ((bytesRead = adbShell.StandardOutput.Read(buffer, 0, buffer.Length)) > 0) 
                {
                    string outputChunk = new string(buffer, 0, bytesRead);
                    Console.Write(outputChunk);

                    // Pengecekan kata diubah jadi huruf kecil semua biar lebih kebal salah baca
                    string lowerChunk = outputChunk.ToLower();
                    if (lowerChunk.Contains("卡密") || lowerChunk.Contains("card") || lowerChunk.Contains("key"))
                    {
                        isWaitingForKey = true;
                    }
                    else if (lowerChunk.Contains("请选择") || lowerChunk.Contains("number") || lowerChunk.Contains("选择"))
                    {
                        isWaitingForKey = false;
                    }
                }
            });

            // 6. PENYADAP INPUT (INTERCEPTOR) - PERBAIKAN LOGIKA SINKRON
            while (!adbShell.HasExited)
            {
                if (Console.KeyAvailable)
                {
                    string input = Console.ReadLine();
                    
                    if (!string.IsNullOrEmpty(input))
                    {
                        bool wasWaiting = isWaitingForKey;
                        isWaitingForKey = false; // Reset status

                        // Langsung kirim ke Android tanpa ditunda
                        adbShell.StandardInput.WriteLine(input);
                        adbShell.StandardInput.Flush();

                        if (wasWaiting)
                        {
                            Console.WriteLine($"\n[C# SYSTEM] >> Key dikirim! Menunggu verifikasi...");
                            
                            // PERBAIKAN: Membekukan C# sejenak untuk menunggu balasan script
                            Thread.Sleep(1500); 
                            
                            if (!isWaitingForKey)
                            {
                                Console.WriteLine($"[C# SYSTEM] >> Verifikasi sukses! Membuka Arena Breakout sekarang...");
                                RunAdbCommand($"shell am start --user {UserId} -n {AppPackage}/{AppActivity}");
                            }
                        }
                        else if (int.TryParse(input, out int pilihanMenu))
                        {
                            Console.WriteLine($"\n[C# SYSTEM] >> Menu {pilihanMenu} dipilih! Memproses...");
                            
                            // PERBAIKAN: Membekukan C# agar script tidak ditutup duluan
                            Thread.Sleep(1500); 
                            
                            if (!isWaitingForKey)
                            {
                                Console.WriteLine($"[C# SYSTEM] >> Eksekusi berhasil! Membuka Arena Breakout sekarang...");
                                RunAdbCommand($"shell am start --user {UserId} -n {AppPackage}/{AppActivity}");
                            }
                        }
                    }
                }
                Thread.Sleep(100); 
            }

            Console.WriteLine("\n✿✿✿✿✿✿✿✿✿ INJECT SELESAI ✿✿✿✿✿✿✿✿✿");
            Console.WriteLine("[~] Sapu-sapu jejak file dari Android...");
            RunAdbCommand($"shell rm -f \"{targetPath}\"");
            
            // PERBAIKAN KRUSIAL: Pkill dihapus dari sini! 
            // Jadi cheat yang sedang berjalan di background tidak akan dibunuh oleh program ini.
        }

        static string RunAdbCommand(string arguments)
        {
            Process process = new Process();
            process.StartInfo.FileName = "adb";
            process.StartInfo.Arguments = arguments;
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.CreateNoWindow = true;
            process.Start();
            
            string output = process.StandardOutput.ReadToEnd();
            process.WaitForExit();
            return output;
        }
    }
}
