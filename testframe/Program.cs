using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace testframe
{
   
    internal static class Program
    {

        private static void ExtractEmbeddedDll(string resourceName ,string target_path)
        {

            // Get the current assembly
            Assembly assembly = Assembly.GetExecutingAssembly();

            // Get the namespace
            string namespaceName = assembly.GetName().Name;

            // Create a temporary file path
            string tempFilePath =  Path.Combine(target_path, resourceName);
            if (File.Exists(tempFilePath))
            {
                File.Delete(tempFilePath);
            }
            // Open the resource stream
            using (Stream stream = assembly.GetManifestResourceStream(namespaceName + "." + resourceName))
            {
                if (stream == null)
                    throw new Exception("Resource not found: " + resourceName);

                // Create the temporary file
                using (FileStream fileStream = new FileStream(tempFilePath, FileMode.Create, FileAccess.Write))
                {
                    stream.CopyTo(fileStream);
                }
            }

           
        }


        /// <summary>
        /// 应用程序的主入口点。
        /// </summary>
        [STAThread]
        static void Main()
        {
            
            ExtractEmbeddedDll("injector.dll", AppDomain.CurrentDomain.BaseDirectory);
            ExtractEmbeddedDll("imgui_hook.dll", "C:\\Windows\\System32");
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new Form1());
       
        }
    }
}
