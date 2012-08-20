using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace GhostGUI
{
    class Incident
    {
        private List<String> LoadedModules = new List<String>();
        private int ProcessID;
        private int ThreadID;

        public Incident(int ProcessID, int ThreadID)
        {
            this.ProcessID = ProcessID;
            this.ThreadID = ThreadID;
        }

        public void AddModule(String ModuleName)
        {
            LoadedModules.Add(ModuleName);
        }

        public IList<String> Modules
        {
            get
            {
                return LoadedModules.AsReadOnly();
            }
        }

        public int PID
        {
            get
            {
                return ProcessID;
            }
        }

        public int TID
        {
            get
            {
                return ThreadID;
            }
        }

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder();
            sb.Append("PID: ").AppendLine(ProcessID.ToString());
            sb.Append("TID: ").AppendLine(ThreadID.ToString());
            sb.AppendLine("Modules:");
            foreach (String s in LoadedModules)
            {
                sb.AppendLine(s);
            }

            return sb.ToString();
        }
    }
}
