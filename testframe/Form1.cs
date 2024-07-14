using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Net;
using System.Reflection.Emit;
using System.Threading;
using System.IO;
namespace testframe
{
    public partial class Form1 : Form
    {





        private BackgroundWorker backgroundWorker;
        string target1 = "Counter-Strike 2";
        string target2 = "反恐精英：全球攻势";
        string dll_name = "imgui_hook.dll";
        int used_target = 0;
        string token_path;


       [DllImport("injector.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int inject_dll_to_window([MarshalAs(UnmanagedType.LPStr)] string window, [MarshalAs(UnmanagedType.LPStr)] string dll);


        [DllImport("injector.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int find_window([MarshalAs(UnmanagedType.LPStr)] string window);



        private void BackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            string target1 = "Counter-Strike 2";
            string target2 = "反恐精英：全球攻势"; 
            this.label1.Text = "等待启动游戏";
            this.Controls.Remove(this.textBox1);
            this.Controls.Remove(this.button1);
            while (true)
            {
                
                if (find_window(target1) == 0)
                {

                    used_target = 1;
                    break;
                }
                if (find_window(target2) == 0)
                {
                    used_target = 2;
                    break;
                }
                Thread.Sleep(200);
            }

            this.label1.Text = "检测到游戏，正在注入";
            int ret = 9;
            if (used_target == 1)
            {
                ret = inject_dll_to_window(target1, dll_name);
            }
            else if (used_target == 2)
            {
                ret = inject_dll_to_window(target2, dll_name);
            }

            if (ret != 0)
            {
                MessageBox.Show("注入失败，请重新打开本程序和游戏");
               
            }
            else
            {
                this.label1.Text = "注入成功,透视已开启（F5）";
                

            }
        }

        private void BackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            
           
        }

        public async Task<int> send_token(string Token )
        {
            using (HttpClient client = new HttpClient())
            {
                var json = "{\"token\": \"" + Token + "\"}";

                // 构造 HttpContent 对象，将 JSON 数据写入请求主体
                var httpContent = new StringContent(json, Encoding.UTF8, "application/json");

                HttpResponseMessage response = await client.PostAsync("http://116.62.188.160:8085/token/check",httpContent);
                if (response.StatusCode == HttpStatusCode.OK)
                {
                    return 0;
                }
                else
                {
                    return 1;
                }
            }
        }
        public Form1()
        {
            InitializeComponent();
            backgroundWorker = new BackgroundWorker();
            backgroundWorker.DoWork += BackgroundWorker_DoWork;
            backgroundWorker.RunWorkerCompleted += BackgroundWorker_RunWorkerCompleted;
            token_path = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "token.ini");
            if (File.Exists(token_path))
            {
                this.textBox1.Text = File.ReadAllText(token_path);
            }
        }

        private void textBox1_TextChanged(object sender, EventArgs e)
        {
            
        }

        private async void button1_Click(object sender, EventArgs e)
        {
            File.WriteAllText(token_path, this.textBox1.Text);
            int valid_token = await send_token(this.textBox1.Text);
            if (valid_token == 1)
            {
                this.label1.Text = "验证失败";
            }
            else
            {
                if (!backgroundWorker.IsBusy)
                {
                    this.button1.Enabled = false;
                    backgroundWorker.RunWorkerAsync();
                }
            }
        }

        private void label1_Click(object sender, EventArgs e)
        {

        }
    }
}
