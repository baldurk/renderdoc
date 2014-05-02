using WeifenLuo.WinFormsUI.Docking.Skins;
using System.ComponentModel;

namespace WeifenLuo.WinFormsUI.Docking
{
    public partial class DockPanel
    {
        private DockPanelSkin m_dockPanelSkin = DockPanelSkinBuilder.Create(Style.VisualStudio2005);
        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockPanel_DockPanelSkin")]
        public DockPanelSkin Skin
        {
            get { return m_dockPanelSkin;  }
            set { m_dockPanelSkin = value; }
        }
        
        private Style m_dockPanelSkinStyle = Style.VisualStudio2005;
        [LocalizedCategory("Category_Docking")]
        [LocalizedDescription("DockPanel_DockPanelSkinStyle")]
        [DefaultValue(Style.VisualStudio2005)]
        public Style SkinStyle
        {
            get { return m_dockPanelSkinStyle; }
            set
            {
                if (m_dockPanelSkinStyle == value)
                    return;

                m_dockPanelSkinStyle = value;

                Skin = DockPanelSkinBuilder.Create(m_dockPanelSkinStyle);
            }
        }
    }
}
