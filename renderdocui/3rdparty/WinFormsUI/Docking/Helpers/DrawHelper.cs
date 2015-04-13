using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Windows.Forms;

namespace WeifenLuo.WinFormsUI.Docking
{
    internal static class DrawHelper
    {
        public static Point RtlTransform(Control control, Point point)
        {
            if (control.RightToLeft != RightToLeft.Yes)
                return point;
            else
                return new Point(control.Right - point.X, point.Y);
        }

        public static Rectangle RtlTransform(Control control, Rectangle rectangle)
        {
            if (control.RightToLeft != RightToLeft.Yes)
                return rectangle;
            else
                return new Rectangle(control.ClientRectangle.Right - rectangle.Right, rectangle.Y, rectangle.Width, rectangle.Height);
        }

        public static GraphicsPath GetRoundedCornerTab(GraphicsPath graphicsPath, Rectangle rect, bool upCorner)
        {
            if (graphicsPath == null)
                graphicsPath = new GraphicsPath();
            else
                graphicsPath.Reset();
            
            int curveSize = 6;
            if (upCorner)
            {
                graphicsPath.AddLine(rect.Left, rect.Bottom, rect.Left, rect.Top + curveSize / 2);
                graphicsPath.AddArc(new Rectangle(rect.Left, rect.Top, curveSize, curveSize), 180, 90);
                graphicsPath.AddLine(rect.Left + curveSize / 2, rect.Top, rect.Right - curveSize / 2, rect.Top);
                graphicsPath.AddArc(new Rectangle(rect.Right - curveSize, rect.Top, curveSize, curveSize), -90, 90);
                graphicsPath.AddLine(rect.Right, rect.Top + curveSize / 2, rect.Right, rect.Bottom);
            }
            else
            {
                graphicsPath.AddLine(rect.Right, rect.Top, rect.Right, rect.Bottom - curveSize / 2);
                graphicsPath.AddArc(new Rectangle(rect.Right - curveSize, rect.Bottom - curveSize, curveSize, curveSize), 0, 90);
                graphicsPath.AddLine(rect.Right - curveSize / 2, rect.Bottom, rect.Left + curveSize / 2, rect.Bottom);
                graphicsPath.AddArc(new Rectangle(rect.Left, rect.Bottom - curveSize, curveSize, curveSize), 90, 90);
                graphicsPath.AddLine(rect.Left, rect.Bottom - curveSize / 2, rect.Left, rect.Top);
            }

            return graphicsPath;
        }

        public static GraphicsPath CalculateGraphicsPathFromBitmap(Bitmap bitmap)
        {
            return CalculateGraphicsPathFromBitmap(bitmap, Color.Empty);
        }

        // From http://edu.cnzz.cn/show_3281.html
        public static GraphicsPath CalculateGraphicsPathFromBitmap(Bitmap bitmap, Color colorTransparent) 
        { 
            GraphicsPath graphicsPath = new GraphicsPath(); 
            if (colorTransparent == Color.Empty)
                colorTransparent = bitmap.GetPixel(0, 0); 

            for(int row = 0; row < bitmap.Height; row ++) 
            { 
                int colOpaquePixel = 0;
                for(int col = 0; col < bitmap.Width; col ++) 
                { 
                    if(bitmap.GetPixel(col, row) != colorTransparent) 
                    { 
                        colOpaquePixel = col; 
                        int colNext = col; 
                        for(colNext = colOpaquePixel; colNext < bitmap.Width; colNext ++) 
                            if(bitmap.GetPixel(colNext, row) == colorTransparent) 
                                break;
 
                        graphicsPath.AddRectangle(new Rectangle(colOpaquePixel, row, colNext - colOpaquePixel, 1)); 
                        col = colNext; 
                    } 
                } 
            } 
            return graphicsPath; 
        } 
    }
}
