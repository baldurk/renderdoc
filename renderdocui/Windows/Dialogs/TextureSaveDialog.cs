using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.IO;
using System.Text;
using System.Windows.Forms;
using renderdoc;
using renderdocui.Code;

namespace renderdocui.Windows.Dialogs
{
    public partial class TextureSaveDialog : Form
    {
        struct AlphaMappingString
        {
            public AlphaMappingString(AlphaMapping v)
            {
                val = v;
            }

            public AlphaMapping val;

            public override string ToString()
            {
                switch (val)
                {
                    case AlphaMapping.Discard:
                        return "Discard";
                    case AlphaMapping.BlendToColour:
                        return "Blend to Colour";
                    case AlphaMapping.BlendToCheckerboard:
                        return "Blend To Checkerboard";
                    case AlphaMapping.Preserve:
                        return "Preserve";
                }

                return "";
            }
        }

        public TextureSaveDialog(Core core)
        {
            InitializeComponent();

            filename.Font =
                fileFormat.Font =
                jpegCompression.Font =
                mipSelect.Font =
                sampleSelect.Font =
                sliceSelect.Font =
                blackPoint.Font =
                whitePoint.Font =
                core.Config.PreferredFont;

            fileFormat.Items.Clear();

            string filter = "";

            foreach (var ft in (FileType[])Enum.GetValues(typeof(FileType)))
            {
                fileFormat.Items.Add(ft.ToString());

                if (filter.Length > 0) filter += "|";
                filter += String.Format("{0} Files (*.{1})|*.{1}", ft.ToString(), ft.ToString().ToLower(Application.CurrentCulture));
            }
            
            saveTexDialog.Filter = filter;

            //if (tex.format == null)
            {
                tex.format = new ResourceFormat();
                tex.format.compCount = 4;
                tex.width = tex.height = 128;
                tex.depth = 1;
                tex.arraysize = 6;
                tex.cubemap = true;
                tex.msSamp = 2;
                tex.mips = 5;
            }
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            if (keyData == Keys.Escape)
            {
                DialogResult = DialogResult.Cancel;
                this.Close();
                return true;
            }
            if (keyData == Keys.Enter)
            {
                ok_Click(this, null);
                return true;
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        public FetchTexture tex = new FetchTexture();
        public TextureSave saveData = new TextureSave();

        public string Filename
        {
            get
            {
                return filename.Text;
            }
        }

        private void TextureSaveDialog_Shown(object sender, EventArgs e)
        {
            jpegCompression.Value = saveData.jpegQuality;

            blackPoint.Text = Formatter.Format(saveData.comp.blackPoint);
            whitePoint.Text = Formatter.Format(saveData.comp.whitePoint);

            mipSelect.Items.Clear();
            for (int i = 0; i < tex.mips; i++)
                mipSelect.Items.Add(i + " - " + Math.Max(1, tex.width >> i) + "x" + Math.Max(1, tex.height >> i));

            mipSelect.SelectedIndex = (saveData.mip >= 0 ? saveData.mip : 0);

            sampleSelect.Items.Clear();
            for (int i = 0; i < tex.msSamp; i++)
                sampleSelect.Items.Add(String.Format("Sample {0}", i));

            sampleSelect.SelectedIndex = Math.Min((int)tex.msSamp, (saveData.sample.sampleIndex == ~0U ? 0 : (int)saveData.sample.sampleIndex));

            resolveSamples.Enabled = true;

            if (tex.format.compType == FormatComponentType.UInt ||
                tex.format.compType == FormatComponentType.SInt ||
                tex.format.compType == FormatComponentType.Depth ||
                (tex.creationFlags & TextureCreationFlags.DSV) != 0)
                resolveSamples.Enabled = false;

            if (saveData.sample.sampleIndex == ~0U && resolveSamples.Enabled)
            {
                resolveSamples.Checked = true;
            }
            else
            {
                oneSample.Checked = true;
            }

            String[] cubeFaces = { "X+", "X-", "Y+", "Y-", "Z+", "Z-" };

            UInt32 numSlices = Math.Max(tex.arraysize, tex.depth);

            sliceSelect.Items.Clear();

            for (UInt32 i = 0; i < numSlices; i++)
            {
                if (tex.cubemap)
                {
                    String name = cubeFaces[i % 6];
                    if (numSlices > 6)
                        name = string.Format("[{0}] {1}", (i / 6), cubeFaces[i % 6]); // Front 1, Back 2, 3, 4 etc for cube arrays
                    sliceSelect.Items.Add(name);
                }
                else
                {
                    sliceSelect.Items.Add("Slice " + i);
                }
            }

            sliceSelect.SelectedIndex = (saveData.slice.sliceIndex >= 0 ? saveData.slice.sliceIndex : 0);

            gridWidth.Maximum = tex.depth * tex.arraysize * tex.msSamp;

            mipGroup.Visible = (tex.mips > 1);

            sampleGroup.Visible = (tex.msSamp > 1);

            sliceGroup.Visible = (tex.depth > 1 || tex.arraysize > 1 || tex.msSamp > 1);

            if (saveData.destType != FileType.DDS)
            {
                cubeCruciform.Enabled = (tex.cubemap && tex.arraysize == 6);
                
                if (!oneSlice.Checked && !cubeCruciform.Enabled)
                    mapSlicesToGrid.Checked = true;
            }

            FileType selectedType = saveData.destType;

            fileFormat.SelectedIndex = 0;
            fileFormat.SelectedIndex = 1;
            fileFormat.SelectedIndex = (int)selectedType;

            if(saveData.alpha == AlphaMapping.Discard)
                alphaMap.SelectedIndex = 0;
            else
                alphaMap.SelectedIndex = alphaMap.Items.Count - 1;
        }

        private void fileFormat_SelectedIndexChanged(object sender, EventArgs e)
        {
            saveData.destType = (FileType)fileFormat.SelectedIndex;

            jpegCompression.Enabled = (saveData.destType == FileType.JPG);

            alphaLDRGroup.Visible = (saveData.destType != FileType.HDR &&
                saveData.destType != FileType.EXR &&
                saveData.destType != FileType.DDS);

            bool noAlphaFormat = (saveData.destType == FileType.BMP || saveData.destType == FileType.JPG);

            // any filetype, PNG supporting or not, can choose to preserve or discard the alpha
            alphaMap.Enabled = tex.format.compCount == 4;

            if (alphaMap.Enabled)
            {
                if (noAlphaFormat && alphaMap.Items.Count != 3)
                {
                    int idx = (int)alphaMap.SelectedIndex;

                    alphaMap.Items.Clear();
                    alphaMap.Items.AddRange(new object[] {
                        new AlphaMappingString(AlphaMapping.Discard),
                        new AlphaMappingString(AlphaMapping.BlendToColour),
                        new AlphaMappingString(AlphaMapping.BlendToCheckerboard)
                    });

                    // if we were discard before, still discard, otherwise blend to checkerboard
                    if (idx <= 0)
                        alphaMap.SelectedIndex = 0;
                    else
                        alphaMap.SelectedIndex = alphaMap.Items.Count - 1;
                }
                else if (alphaMap.Items.Count != 2)
                {
                    int idx = (int)alphaMap.SelectedIndex;

                    alphaMap.Items.Clear();
                    alphaMap.Items.AddRange(new object[] {
                        new AlphaMappingString(AlphaMapping.Discard),
                        new AlphaMappingString(AlphaMapping.Preserve)
                    });

                    // allow the previous selection to clamp, to either discard or preserve
                    alphaMap.SelectedIndex = Helpers.Clamp(idx, 0, alphaMap.Items.Count-1);
                }
            }

            if (alphaMap.Items.Count == 0)
            {
                alphaMap.Items.Clear();
                alphaMap.Items.AddRange(new object[] {
                    new AlphaMappingString(AlphaMapping.Discard),
                    new AlphaMappingString(AlphaMapping.Preserve)
                });
            }

            alphaCol.Enabled = (saveData.alpha == AlphaMapping.BlendToColour && tex.format.compCount == 4 && noAlphaFormat);

            if (saveData.destType == FileType.DDS)
            {
                exportAllMips.Enabled = exportAllMips.Checked = true;
                exportAllSlices.Enabled = exportAllSlices.Checked = true;

                cubeCruciform.Enabled = cubeCruciform.Checked = false;
                gridWidth.Enabled = mapSlicesToGrid.Enabled = mapSlicesToGrid.Checked = false;
            }
            else
            {
                exportAllMips.Enabled = false;
                oneMip.Checked = oneSlice.Checked = true;
            }
            SetFilenameFromFiletype();
        }

        private void jpegCompression_ValueChanged(object sender, EventArgs e)
        {
            saveData.jpegQuality = (int)jpegCompression.Value;
        }

        private void gridWidth_ValueChanged(object sender, EventArgs e)
        {
            saveData.slice.sliceGridWidth = (int)gridWidth.Value;
        }

        private void alphaMap_SelectedIndexChanged(object sender, EventArgs e)
        {
            saveData.alpha = ((AlphaMappingString)alphaMap.SelectedItem).val;

            alphaCol.Enabled = (saveData.alpha == AlphaMapping.BlendToColour);
        }

        private void mipSelect_SelectedIndexChanged(object sender, EventArgs e)
        {
            saveData.mip = (int)mipSelect.SelectedIndex;
        }

        private void sampleSelect_SelectedIndexChanged(object sender, EventArgs e)
        {
            saveData.sample.sampleIndex = (uint)sampleSelect.SelectedIndex;
        }

        private void sliceSelect_SelectedIndexChanged(object sender, EventArgs e)
        {
            saveData.slice.sliceIndex = (int)sliceSelect.SelectedIndex;
        }

        private void alphaCol_Click(object sender, EventArgs e)
        {
            var res = colorDialog.ShowDialog();

            if (res == DialogResult.OK || res == DialogResult.Yes)
            {
                saveData.alphaCol = new FloatVector(
                        ((float)colorDialog.Color.R) / 255.0f,
                        ((float)colorDialog.Color.G) / 255.0f,
                        ((float)colorDialog.Color.B) / 255.0f);
            }
        }

        private void SetFiletypeFromFilename()
        {
            try
            {
                string ext = Path.GetExtension(filename.Text).ToUpperInvariant().Substring(1); // trim . from extension

                foreach (var ft in (FileType[])Enum.GetValues(typeof(FileType)))
                {
                    if (ft.ToString().ToUpperInvariant() == ext)
                    {
                        fileFormat.SelectedIndex = (int)ft;
                        break;
                    }
                }
            }
            catch (ArgumentException)
            {
                // invalid path or similar
            }
        }

        private void SetFilenameFromFiletype()
        {
            try
            {
                string filenameExt = Path.GetExtension(filename.Text).ToLowerInvariant().Substring(1); // trim . from extension

                FileType[] types = (FileType[])Enum.GetValues(typeof(FileType));

                string selectedExt = types[fileFormat.SelectedIndex].ToString().ToLowerInvariant();

                if (selectedExt != filenameExt)
                {
                    filename.Text = filename.Text.Substring(0, filename.Text.Length - filenameExt.Length);
                    filename.Text += selectedExt;
                }
            }
            catch (ArgumentException)
            {
                // invalid path or similar
            }
        }

        private void browse_Click(object sender, EventArgs e)
        {
            saveTexDialog.FilterIndex = fileFormat.SelectedIndex + 1;
            var res = saveTexDialog.ShowDialog();
            if (res == DialogResult.OK || res == DialogResult.Yes)
            {
                filename.Text = saveTexDialog.FileName;
                SetFiletypeFromFilename();
            }
        }

        private void ok_Click(object sender, EventArgs e)
        {
            saveData.alpha = ((AlphaMappingString)alphaMap.SelectedItem).val;

            if (saveData.alpha == AlphaMapping.BlendToCheckerboard)
            {
                saveData.alphaCol = new FloatVector(0.666f, 0.666f, 0.666f);
            }

            if (exportAllMips.Checked)
                saveData.mip = -1;
            else
                saveData.mip = (int)mipSelect.SelectedIndex;

            if (resolveSamples.Checked)
            {
                saveData.sample.sampleIndex = ~0U;
                saveData.sample.mapToArray = false;
            }
            else if (mapSampleArray.Checked)
            {
                saveData.sample.sampleIndex = 0;
                saveData.sample.mapToArray = true;
            }
            else
            {
                saveData.sample.sampleIndex = (uint)sampleSelect.SelectedIndex;
                saveData.sample.mapToArray = false;
            }

            if (!exportAllSlices.Checked)
            {
                saveData.slice.cubeCruciform = saveData.slice.slicesAsGrid = false;
                saveData.slice.sliceGridWidth = 1;
                saveData.slice.sliceIndex = (int)sliceSelect.SelectedIndex;
            }
            else
            {
                saveData.slice.sliceIndex = -1;
                if (cubeCruciform.Checked)
                {
                    saveData.slice.cubeCruciform = true;
                    saveData.slice.slicesAsGrid = false;
                    saveData.slice.sliceGridWidth = 1;
                }
                else
                {
                    saveData.slice.cubeCruciform = false;
                    saveData.slice.slicesAsGrid = true;
                    saveData.slice.sliceGridWidth = (int)gridWidth.Value;
                }
            }

            saveData.destType = (FileType)fileFormat.SelectedIndex;
            saveData.jpegQuality = (int)jpegCompression.Value;

            float.TryParse(blackPoint.Text, out saveData.comp.blackPoint);
            float.TryParse(whitePoint.Text, out saveData.comp.whitePoint);

            try
            {
                // use same path for non-existing path as invalid path
                if (!Directory.Exists(Path.GetDirectoryName(Filename)))
                    throw new ArgumentException();

                if (File.Exists(Filename))
                {
                    var res = MessageBox.Show(String.Format("{0} already exists.\nDo you want to replace it?", Path.GetFileName(Filename)), "Confirm Save Texture",
                                MessageBoxButtons.YesNo, MessageBoxIcon.Exclamation, MessageBoxDefaultButton.Button2);

                    if (res != DialogResult.Yes)
                        return;
                }
            }
            catch(ArgumentException)
            {
                // invalid path or similar

                MessageBox.Show(String.Format("{0}\nPath does not exist.\nCheck the path and try again.", Filename), "Save Texture",
                                MessageBoxButtons.OK, MessageBoxIcon.Exclamation, MessageBoxDefaultButton.Button1);

                return;
            }

            // path is valid and either doesn't exist or user confirmed replacement
            DialogResult = DialogResult.OK;
            Close();
        }

        private void blackPoint_TextChanged(object sender, EventArgs e)
        {
            float.TryParse(blackPoint.Text, out saveData.comp.blackPoint);
        }

        private void whitePoint_TextChanged(object sender, EventArgs e)
        {
            float.TryParse(whitePoint.Text, out saveData.comp.whitePoint);
        }

        private bool recurse = false;

        // a horrible mess of functions to try and maintain valid combinations of options for different
        // filetypes etc. There might be a better way of doing this...

        private void exportAllMips_CheckedChanged(object sender, EventArgs e)
        {
            if (recurse) return;
            recurse = true;
            oneMip.Checked = !exportAllMips.Checked;
            mipSelect.Enabled = oneMip.Checked;
            recurse = false;
        }

        private void oneMip_CheckedChanged(object sender, EventArgs e)
        {
            if (recurse) return;
            recurse = true;
            exportAllMips.Checked = !oneMip.Checked;
            mipSelect.Enabled = oneMip.Checked;

            if (saveData.destType != FileType.DDS)
            {
                oneMip.Checked = true;
                exportAllMips.Checked = false;
                mipSelect.Enabled = true;
            }

            recurse = false;
        }

        private void mapSampleArray_CheckedChanged(object sender, EventArgs e)
        {
            if (recurse) return;
            recurse = true;
            if (mapSampleArray.Checked)
            {
                resolveSamples.Checked = oneSample.Checked = false;
            }
            else
            {
                resolveSamples.Checked = false;
                oneSample.Checked = true;
            }
            sampleSelect.Enabled = oneSample.Checked;
            recurse = false;
        }

        private void resolveSamples_CheckedChanged(object sender, EventArgs e)
        {
            if (recurse) return;
            recurse = true;
            if (resolveSamples.Checked)
            {
                mapSampleArray.Checked = oneSample.Checked = false;
            }
            else
            {
                mapSampleArray.Checked = false;
                oneSample.Checked = true;
            }
            sampleSelect.Enabled = oneSample.Checked;
            recurse = false;
        }

        private void oneSample_CheckedChanged(object sender, EventArgs e)
        {
            if (recurse) return;
            recurse = true;
            if (oneSample.Checked)
            {
                mapSampleArray.Checked = resolveSamples.Checked = false;
            }
            else
            {
                mapSampleArray.Checked = false;
                resolveSamples.Checked = true;
            }
            sampleSelect.Enabled = oneSample.Checked;
            recurse = false;
        }

        private void mapSlicesToGrid_CheckedChanged(object sender, EventArgs e)
        {
            if (recurse) return;
            recurse = true;
            if (mapSlicesToGrid.Checked)
            {
                cubeCruciform.Checked = false;
            }
            else if (saveData.destType != FileType.DDS)
            {
                oneSlice.Checked = true;
                exportAllSlices.Checked = false;
                cubeCruciform.Enabled = mapSlicesToGrid.Enabled = gridWidth.Enabled = false;
                sliceSelect.Enabled = true;
            }
            recurse = false;
            if (saveData.destType == FileType.DDS)
                gridWidth.Enabled = false;
            else
                gridWidth.Enabled = mapSlicesToGrid.Checked;
        }

        private void cubeCruciform_CheckedChanged(object sender, EventArgs e)
        {
            if (recurse) return;
            recurse = true;
            if (cubeCruciform.Checked)
            {
                mapSlicesToGrid.Checked = false;
            }
            else if (saveData.destType != FileType.DDS)
            {
                oneSlice.Checked = true;
                exportAllSlices.Checked = false;
                cubeCruciform.Enabled = mapSlicesToGrid.Enabled = gridWidth.Enabled = false;
                sliceSelect.Enabled = true;
            }
            recurse = false;
        }

        private void oneSlice_CheckedChanged(object sender, EventArgs e)
        {
            if (recurse) return;
            recurse = true;
            exportAllSlices.Checked = !oneSlice.Checked;
            if (saveData.destType == FileType.DDS)
            {
                mapSlicesToGrid.Enabled = gridWidth.Enabled = cubeCruciform.Enabled = false;
            }
            else
            {
                mapSlicesToGrid.Enabled = gridWidth.Enabled = !oneSlice.Checked;

                if (!oneSlice.Checked && !cubeCruciform.Checked)
                    mapSlicesToGrid.Checked = true;

                if (tex.cubemap && tex.arraysize == 6) cubeCruciform.Enabled = !oneSlice.Checked;
                else cubeCruciform.Enabled = false;
            }
            sliceSelect.Enabled = oneSlice.Checked;
            recurse = false;
        }

        private void exportAllSlices_CheckedChanged(object sender, EventArgs e)
        {
            if (recurse) return;
            recurse = true;
            oneSlice.Checked = !exportAllSlices.Checked;
            if (saveData.destType == FileType.DDS)
            {
                mapSlicesToGrid.Enabled = gridWidth.Enabled = cubeCruciform.Enabled = false;
            }
            else
            {
                mapSlicesToGrid.Enabled = gridWidth.Enabled = !oneSlice.Checked;

                if (!oneSlice.Checked && !cubeCruciform.Checked)
                    mapSlicesToGrid.Checked = true;

                if (tex.cubemap && tex.arraysize == 6) cubeCruciform.Enabled = !oneSlice.Checked;
                else cubeCruciform.Enabled = false;
            }
            sliceSelect.Enabled = oneSlice.Checked;
            recurse = false;
        }

        private void filename_KeyUp(object sender, KeyEventArgs e)
        {
            typingTimer.Enabled = true;
            typingTimer.Stop();
            typingTimer.Start();
        }

        private void typingTimer_Tick(object sender, EventArgs e)
        {
            SetFiletypeFromFilename();
            typingTimer.Enabled = false;
        }
    }
}
