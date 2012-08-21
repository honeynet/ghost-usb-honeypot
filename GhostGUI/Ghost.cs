using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace GhostGUI
{
    class Ghost
    {
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

        public delegate void GhostUpdateHandler();
        public event GhostUpdateHandler OnUpdate;

        public void Start()
        {
            if (DeviceState != State.BeforeOperation)
            {
                throw new Exception("Device state invalid for this operation");
            }

            DeviceID = GhostMountDevice(IncidentCreator, IntPtr.Zero);
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

        public IList<Incident> IncidentList
        {
            get
            {
                return Incidents.AsReadOnly();
            }
        }

        public DateTime MountTime
        {
            get
            {
                return Mount;
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
        private static extern int GhostMountDevice(GhostIncidentCallback Callback, IntPtr Context);
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
