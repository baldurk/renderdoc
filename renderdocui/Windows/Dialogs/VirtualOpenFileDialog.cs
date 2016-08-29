using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using renderdoc;
using renderdocui.Code;
using System.Text.RegularExpressions;

namespace renderdocui.Windows.Dialogs
{
    public partial class VirtualOpenFileDialog : Form
    {
        private const int ICON_DRIVE = 0;
        private const int ICON_DIRECTORY = 1;
        private const int ICON_DIRECTORY_FADED = 2;
        private const int ICON_FILE = 3;
        private const int ICON_FILE_FADED = 4;
        private const int ICON_EXECUTABLE = 5;
        private const int ICON_EXECUTABLE_FADED = 6;

        private const int TYPE_EXES = 0;
        private const int TYPE_ALL = 1;

        private RenderManager m_RM = null;
        private List<DirectoryFileTreeNode> currentFiles = new List<DirectoryFileTreeNode>();
        private string currentDir = "";
        private List<string> history = new List<string>();
        private int historyidx = 0;

        private List<DirectoryFileTreeNode> roots = new List<DirectoryFileTreeNode>();

        #region Events

        private static readonly object FileOkEvent = new object();
        public event EventHandler<FileOpenedEventArgs> Opened
        {
            add { Events.AddHandler(FileOkEvent, value); }
            remove { Events.RemoveHandler(FileOkEvent, value); }
        }
        protected virtual void OnOpened(FileOpenedEventArgs e)
        {
            EventHandler<FileOpenedEventArgs> handler = (EventHandler<FileOpenedEventArgs>)Events[FileOkEvent];
            if (handler != null)
                handler(this, e);
        }

        #endregion

        public VirtualOpenFileDialog(RenderManager rm)
        {
            InitializeComponent();

            Icon = global::renderdocui.Properties.Resources.icon;

            m_RM = rm;

            showHidden.Checked = false;
            fileType.SelectedIndex = 0;
            back.Enabled = forward.Enabled = up.Enabled = false;
        }

        private string Directory { get { return currentDir.Replace('\\', '/'); } }
        private string FileName { get { return filename.Text.Trim(); } }

        private bool m_DirBrowse = false;
        public bool DirectoryBrowse
        {
            set
            {
                m_DirBrowse = value;
                Text = "Browse for folder";
                open.Text = "Select folder";
            }
        }

        private string ChosenPath
        {
            get
            {
                return Directory + "/" + FileName;
            }
        }

        private bool NTPaths = false;

        private bool IsRoot
        {
            get
            {
                // X: (we remove trailing slashes)
                return NTPaths ? currentDir.Length == 2 : currentDir == "/";
            }
        }

        private bool RemoteFailed = false;

        private void CheckRemote(bool success)
        {
            if (!success && !RemoteFailed)
            {
                MessageBox.Show("Remote server disconnected during browsing.\n" +
                    "You must close this file browser and restore the connection before continuing.",
                    "Remote server disconnected",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);

                RemoteFailed = true;

                BeginInvoke(new Action(UpdateViewsFromData));
            }
        }

        private DirectoryFileTreeNode GetNode(string path)
        {
            if (NTPaths)
            {
                for (int i = 0; i < roots.Count; i++)
                {
                    if (roots[i].Filename == path || roots[i].Filename[0] + ":" == path)
                        return roots[i];

                    if (roots[i].Filename[0] == path[0])
                        return roots[i].GetNode(path.Substring(3));
                }

                return null;
            }
            else
            {
                if (path == "/" || path == "")
                    return roots[0];

                return roots[0].GetNode(path.Substring(1));
            }
        }

        private void InitialPopulate()
        {
            bool populated = false;

            m_RM.GetHomeFolder((string path, DirectoryFile[] f) =>
            {
                currentDir = path;
                history.Add(currentDir);

                // check if we're NT paths or unix paths, and populate the roots
                if (currentDir[0].IsAlpha() && currentDir[1] == ':')
                {
                    NTPaths = true;

                    CheckRemote(m_RM.ListFolder("/", (string p, DirectoryFile[] files) =>
                    {
                        foreach (var root in files)
                        {
                            DirectoryFileTreeNode node = new DirectoryFileTreeNode();
                            node.Path = node.Filename = root.filename;
                            node.IsDirectory = true;
                            roots.Add(node);

                            CheckRemote(m_RM.ListFolder(root.filename, (string a, DirectoryFile[] b) => { node.Populate(a, b); }));
                        }
                    }));
                }
                else
                {
                    NTPaths = false;

                    DirectoryFileTreeNode node = new DirectoryFileTreeNode();
                    node.Path = node.Filename = "/";
                    node.IsDirectory = true;
                    roots.Add(node);
                   CheckRemote(m_RM.ListFolder("/", (string a, DirectoryFile[] b) => { node.Populate(a, b); }));
                }

                // we will populate the rest of the folders when expanding to the current directory

                populated = true;

                BeginInvoke(new Action(UpdateViewsFromData));
            });

            // wait a second for the results to come in
            SpinWait.SpinUntil((Func<bool>)(() => { return populated; }), 1000);
        }

        private void PopulateNode(TreeNode treenode)
        {
            DirectoryFileTreeNode node = treenode.Tag as DirectoryFileTreeNode;

            if (node.ChildrenPopulated || node.children.Count == 0)
                return;

            node.ChildrenPopulated = true;

            CheckRemote(m_RM.ListFolder(node.children[0].Path, (string path, DirectoryFile[] files) =>
            {
                node.children[0].Populate(path, files);

                // do the rest on-thread
                for(int i = 1; i < node.children.Count; i++)
                {
                    if (!node.children[i].IsDirectory || node.children[i].HasPopulated)
                        continue;

                    m_RM.ListFolder(node.children[i].Path, (string a, DirectoryFile[] b) =>
                    {
                        node.children[i].Populate(a, b);
                    });
                }
            }));

            BeginInvoke(new Action(UpdateViewsFromData));
        }

        private void AddDirTreeNodeRecursive(DirectoryFileTreeNode node, TreeNode parent)
        {
            if (!node.IsDirectory)
                return;

            if (node.directoryNode == null)
            {
                TreeNode treenode = new TreeNode();
                treenode.Text = node.Filename;
                treenode.SelectedImageIndex = treenode.ImageIndex = ICON_DIRECTORY;
                treenode.Tag = node;
                node.directoryNode = treenode;

                parent.Nodes.Add(treenode);
            }

            foreach (var child in node.children)
            {
                AddDirTreeNodeRecursive(child, node.directoryNode);
            }
        }

        bool insideUpdateViews = false;

        private void UpdateViewsFromData()
        {
            if(insideUpdateViews)
                return;

            insideUpdateViews = true;

            location.Text = Directory;

            if (RemoteFailed)
            {
                directoryTree.BeginUpdate();
                directoryTree.Nodes.Clear();
                directoryTree.EndUpdate();

                up.Enabled = back.Enabled = forward.Enabled =
                    showHidden.Enabled = fileType.Enabled = open.Enabled =
                    filename.Enabled = location.Enabled = false;

                location.Text = filename.Text = "";

                UpdateFileList();
                return;
            }

            directoryTree.BeginUpdate();

            foreach (var root in roots)
            {
                if (root.directoryNode == null)
                {
                    TreeNode treenode = new TreeNode();
                    treenode.Text = root.Filename;
                    treenode.SelectedImageIndex = treenode.ImageIndex = ICON_DRIVE;
                    treenode.Tag = root;
                    root.directoryNode = treenode;

                    directoryTree.Nodes.Add(treenode);
                }

                AddDirTreeNodeRecursive(root, root.directoryNode);
            }

            directoryTree.EndUpdate();

            string[] components = Directory.Split('/');

            DirectoryFileTreeNode node = null;

            // expand down to the current directory
            for (int i = 1; i < components.Length; i++)
            {
                string dir = NTPaths ? components[0] : "";

                for (int j = 1; j < i; j++)
                    dir += "/" + components[j];

                if (dir == "")
                    dir = "/";

                node = GetNode(dir);
                if (node == null || node.directoryNode == null)
                    break;
                node.directoryNode.Expand();
            }

            node = GetNode(Directory);
            if (node != null && node.directoryNode != null)
                directoryTree.SelectedNode = node.directoryNode;

            UpdateFileList();

            up.Enabled = !IsRoot;
            back.Enabled = history.Count > 0 && historyidx > 0;
            forward.Enabled = historyidx < history.Count - 1;

            insideUpdateViews = false;
        }

        private bool ChangeDirectory(string dir, bool recordHistory)
        {
            if (Directory == dir)
                return true;

            // normalise input
            dir = dir.Replace('\\', '/');
            if (dir.Length > 0 && dir[dir.Length - 1] == '/')
                dir = dir.Substring(0, dir.Length - 1);

            if (NTPaths)
                dir = dir.Replace("://", ":/");

            DirectoryFileTreeNode node = GetNode(dir);

            if (node != null && node.AccessDenied)
            {
                ShowAccessDeniedMessage(dir);
                return false;
            }

            if (node == null)
            {
                bool immediateError = false;

                CheckRemote(m_RM.ListFolder(dir, (string a, DirectoryFile[] b) =>
                {
                    if (b.Length == 1 && b[0].flags.HasFlag(DirectoryFileProperty.ErrorInvalidPath))
                    {
                        ShowFileNotFoundMessage(dir);
                        immediateError = true;
                        return;
                    }

                    if (b.Length == 1 && b[0].flags.HasFlag(DirectoryFileProperty.ErrorAccessDenied))
                    {
                        ShowAccessDeniedMessage(dir);
                        immediateError = true;
                        return;
                    }

                    if (recordHistory)
                    {
                        if (historyidx < history.Count - 1)
                            history.RemoveRange(historyidx, history.Count - 1 - historyidx);

                        history.Add(dir);
                        historyidx = history.Count - 1;
                    }
                    currentDir = dir;

                    UpdateViewsFromData();
                }));

                if (immediateError)
                    return false;

                return true;
            }

            if (recordHistory)
            {
                if (historyidx < history.Count - 1)
                    history.RemoveRange(historyidx, history.Count - 1 - historyidx);

                history.Add(dir);
                historyidx = history.Count - 1;
            }
            currentDir = dir;

            UpdateViewsFromData();

            return true;
        }

        private bool ChangeDirectory(string dir)
        {
            return ChangeDirectory(dir, true);
        }

        private void UpdateFileList()
        {
            fileList.BeginUpdate();
            fileList.Clear();
            fileList.EndUpdate();

            currentFiles.Clear();

            // return, we haven't populated anything yet
            if (roots.Count == 0 || RemoteFailed)
                return;

            DirectoryFileTreeNode node = GetNode(Directory);

            if (node == null)
                return;

            string match = ".*";
            
            if (FileName.IndexOf('*') != -1 || FileName.IndexOf('?') != -1)
                match = Regex.Escape(FileName).Replace( @"\*", ".*" ).Replace( @"\?", "." );

            fileList.BeginUpdate();

            foreach (var child in node.children)
            {
                if (child.IsHidden && !showHidden.Checked)
                    continue;

                var res = Regex.Match(child.Filename, match);

                // always display directories even if they don't match a glob
                if (!res.Success && !child.IsDirectory)
                    continue;

                int imgidx = ICON_FILE;
                if (child.IsDirectory)
                    imgidx = ICON_DIRECTORY;
                else if (child.IsExecutable)
                    imgidx = ICON_EXECUTABLE;

                // skip non-executables
                if(imgidx == ICON_FILE && fileType.SelectedIndex == 0)
                    continue;

                if (child.IsHidden)
                {
                    imgidx = ICON_FILE_FADED;
                    if (child.IsDirectory)
                        imgidx = ICON_DIRECTORY_FADED;
                    else if (child.IsExecutable)
                        imgidx = ICON_EXECUTABLE_FADED;
                }

                ListViewItem item = new ListViewItem(child.Filename, imgidx);
                item.Tag = child;

                fileList.Items.Add(item);

                currentFiles.Add(child);
            }

            fileList.EndUpdate();
        }

        private bool TryOpenFile(bool doubleclick)
        {
            // double clicking enters directories, it doesn't choose them
            if (m_DirBrowse && !doubleclick)
            {
                DirectoryFileTreeNode node = GetNode(Directory);

                if (node != null)
                {
                    OnOpened(new FileOpenedEventArgs(NTPaths, Directory));
                    Close();

                    return true;
                }

                return false;
            }

            int idx = currentFiles.FindIndex(f => f.Filename == FileName);
            if (idx >= 0)
            {
                if (currentFiles[idx].IsDirectory)
                {
                    ChangeDirectory(Directory + "/" + currentFiles[idx].Filename);
                    return true;
                }

                OnOpened(new FileOpenedEventArgs(NTPaths, ChosenPath));
                Close();

                return true;
            }

            return false;
        }

        private void ShowFileNotFoundMessage(string fn)
        {
            MessageBox.Show(String.Format("{0}\nFile not found.\nCheck the file name and try again.", fn), "File not found", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
        }

        private void ShowAccessDeniedMessage(string fn)
        {
            MessageBox.Show(String.Format("{0} is not accessible\n\nAccess is denied.", fn), "Access is denied", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }

        #region Handlers

        private void VirtualOpenFileDialog_Load(object sender, EventArgs e)
        {
            InitialPopulate();
        }

        private void showHidden_CheckedChanged(object sender, EventArgs e)
        {
            UpdateFileList();
        }

        private void fileType_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateFileList();
        }

        private void filename_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == '\r' || e.KeyChar == '\n')
            {
                if (TryOpenFile(false))
                    return;

                if (FileName.IndexOf('*') == -1 && FileName.IndexOf('?') == -1)
                {
                    ShowFileNotFoundMessage(FileName);
                    return;
                }

                UpdateFileList();
            }
        }

        private void open_Click(object sender, EventArgs e)
        {
            if (TryOpenFile(false))
                return;
            else if (FileName != "")
                ShowFileNotFoundMessage(FileName);
        }

        private void cancel_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void location_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == '\r' || e.KeyChar == '\n')
            {
                ChangeDirectory(location.Text);
            }
        }

        private void directoryTree_BeforeSelect(object sender, TreeViewCancelEventArgs e)
        {
            // for unix paths we need to drop the leading /
            bool success = true;

            if(!NTPaths)
                success = ChangeDirectory(e.Node.FullPath.Substring(1));
            else
                success = ChangeDirectory(e.Node.FullPath);

            if (!success)
                e.Cancel = true;
        }

        private void directoryTree_BeforeExpand(object sender, TreeViewCancelEventArgs e)
        {
            PopulateNode(e.Node);
        }

        private void back_Click(object sender, EventArgs e)
        {
            historyidx--;
            ChangeDirectory(history[historyidx], false);
        }

        private void forward_Click(object sender, EventArgs e)
        {
            historyidx++;
            ChangeDirectory(history[historyidx], false);
        }

        private void up_Click(object sender, EventArgs e)
        {
            string dir = Directory;

            int idx = dir.LastIndexOf('/');

            if (idx == 0)
                ChangeDirectory("/");
            else if (idx >= 0)
                ChangeDirectory(dir.Substring(0, idx));
        }

        private void fileList_DoubleClick(object sender, EventArgs e)
        {
            if(fileList.SelectedItems.Count == 1)
            {
                filename.Text = fileList.SelectedItems[0].Text;

                if (TryOpenFile(true))
                    return;
                else
                    ShowFileNotFoundMessage(FileName);
            }
        }

        private void fileList_SelectedIndexChanged(object sender, EventArgs e)
        {
            if(fileList.SelectedItems.Count > 0)
                filename.Text = fileList.SelectedItems[0].Text;
        }

        #endregion
    }

    public class FileOpenedEventArgs : EventArgs
    {
        private string m_fn;

        public FileOpenedEventArgs(bool NT, string fn)
        {
            if(NT)
                m_fn = fn.Replace('/', '\\');
            else
                m_fn = fn;
        }

        public string FileName
        {
            get { return m_fn; }
        }
    }

    public class DirectoryFileTreeNode
    {
        public string Filename = "";
        public string Path = "";
        public bool IsHidden = false;
        public bool IsExecutable = false;
        public bool IsDirectory = false;

        public bool AccessDenied = false;

        public TreeNode directoryNode = null;

        public bool HasPopulated = false;
        public bool ChildrenPopulated = false;
        public List<DirectoryFileTreeNode> children = new List<DirectoryFileTreeNode>();

        public void Populate(string path, DirectoryFile[] files)
        {
            HasPopulated = true;

            if (files.Length == 1 && files[0].flags.HasFlag(DirectoryFileProperty.ErrorAccessDenied))
            {
                AccessDenied = true;
                return;
            }

            foreach (DirectoryFile file in files)
            {
                DirectoryFileTreeNode child = new DirectoryFileTreeNode();
                child.Filename = file.filename;
                if (Path == "/")
                    child.Path = Path + file.filename;
                else
                    child.Path = Path + "/" + file.filename;
                child.IsHidden = file.flags.HasFlag(DirectoryFileProperty.Hidden);
                child.IsExecutable = file.flags.HasFlag(DirectoryFileProperty.Executable);
                child.IsDirectory = file.flags.HasFlag(DirectoryFileProperty.Directory);
                children.Add(child);
            }
        }

        public DirectoryFileTreeNode GetNode(string path)
        {
            int idx = path.IndexOf('/');

            string dirname = "";
            string rest = "";

            if (idx >= 0)
            {
                rest = path.Substring(idx + 1);
                dirname = path.Substring(0, idx);
            }

            foreach (DirectoryFileTreeNode child in children)
            {
                if (child.Filename == path)
                    return child;
                else if (child.Filename == dirname)
                    return child.GetNode(rest);
            }

            return null;
        }

        public override string ToString()
        {
            return Path;
        }
    }
}
