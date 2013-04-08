using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace GhostGUI
{
    [Serializable]
    public class Ghost
    {
        protected const string ParamRegPath = "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ghostdrive\\Parameters";
        protected const string ImageSubKey = "ImageFileName9";
        protected static string GhostImageFileName = (String)Microsoft.Win32.Registry.GetValue(ParamRegPath, ImageSubKey, "");

        protected const int GhostDeviceID = 9;
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        protected delegate void GhostIncidentCallback(int DeviceID, int IncidentID, IntPtr Context);
        protected enum State
        {
            BeforeOperation,
            Mounted,
            AfterOperation
        }

        protected List<Incident> Incidents = new List<Incident>();
        protected int DeviceID;
        protected State DeviceState = State.BeforeOperation;
        protected DateTime Mount;
        private GhostIncidentCallback Callback;

        public delegate void GhostUpdateHandler();
        public event GhostUpdateHandler OnUpdate;

        public static string ImageFileName
        {
            get
            {
                return GhostImageFileName;
            }
            set
            {
                GhostImageFileName = value;
                Microsoft.Win32.Registry.SetValue(ParamRegPath, ImageSubKey, value);
            }
        }

        public void Start()
        {
            if (DeviceState != State.BeforeOperation)
            {
                throw new Exception("Device state invalid for this operation");
            }

            Callback = new GhostIncidentCallback(IncidentCreator);
            DeviceID = GhostMountDeviceWithID(GhostDeviceID, Callback, IntPtr.Zero);
            if (DeviceID == -1)
            {
                int Error = GhostGetLastError();
                throw new Exception(GhostGetErrorDescription(Error));
            }

            DeviceState = State.Mounted;
            Mount = DateTime.Now;
            OnUpdate();
        }

        public void Stop()
        {
            if (DeviceState != State.Mounted)
            {
                throw new Exception("Device state invalid for this operation");
            }

            if (GhostUmountDevice(DeviceID) == -1)
            {
                int Error = GhostGetLastError();
                throw new Exception(GhostGetErrorDescription(Error));
            }

            DeviceState = State.AfterOperation;
        }

        public List<Incident> IncidentList
        {
            get
            {
                return Incidents;
            }
            set
            {
                Incidents = value;
            }
        }

        public DateTime MountTime
        {
            get
            {
                return Mount;
            }
            set
            {
                Mount = value;
            }
        }

        private void IncidentCreator(int DeviceID, int IncidentID, IntPtr Context)
        {
            int ProcessID = GhostGetProcessID(DeviceID, IncidentID);
            int ThreadID = GhostGetThreadID(DeviceID, IncidentID);
            Incident NewIncident = new Incident(ProcessID, ThreadID);

            int NumModules = GhostGetNumModules(DeviceID, IncidentID);
            for (int i = 0; i < NumModules; i++)
            {
                StringBuilder ModuleName = new StringBuilder(256);
                if (GhostGetModuleName(DeviceID, IncidentID, i, ModuleName, ModuleName.Capacity) == -1)
                {
                    int Error = GhostGetLastError();
                    throw new Exception(GhostGetErrorDescription(Error));
                }

                NewIncident.AddModule(ModuleName.ToString());
            }

            Incidents.Add(NewIncident);
            OnUpdate();
        }

        [DllImport("ghostlib.dll")]
        private static extern int GhostMountDeviceWithID(int DeviceID, GhostIncidentCallback Callback, IntPtr Context);
        [DllImport("ghostlib.dll")]
        private static extern int GhostUmountDevice(int DeviceID);
        [DllImport("ghostlib.dll")]
        private static extern int GhostGetLastError();
        [DllImport("ghostlib.dll")]
        private static extern String GhostGetErrorDescription(int Error);
        [DllImport("ghostlib.dll")]
        private static extern int GhostGetProcessID(int DeviceID, int IncidentID);
        [DllImport("ghostlib.dll")]
        private static extern int GhostGetThreadID(int DeviceID, int IncidentID);
        [DllImport("ghostlib.dll")]
        private static extern int GhostGetNumModules(int DeviceID, int IncidentID);
        [DllImport("ghostlib.dll", CharSet = CharSet.Unicode)]
        private static extern int GhostGetModuleName(int DeviceID, int IncidentID, int ModuleIndex, StringBuilder ModuleName, int ModuleNameSize);
    }
}
