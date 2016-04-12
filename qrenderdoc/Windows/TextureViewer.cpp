#include "TextureViewer.h"
#include "ui_TextureViewer.h"

#include "FlowLayout.h"

#include "Code/Core.h"

#if defined(__linux__)
#include <QX11Info>
#include <xcb/xcb.h>
#include <GL/glx.h>
#endif

TextureViewer::TextureViewer(Core *core, QWidget *parent) :
QFrame(parent),
ui(new Ui::TextureViewer),
m_Core(core)
{
	ui->setupUi(this);

	m_Core->AddLogViewer(this);

	ui->render->SetOutput(NULL);
	ui->pixelContext->SetOutput(NULL);
	m_Output = NULL;

	QWidget *renderContainer = ui->renderContainer;

    QObject::connect(ui->render, &CustomPaintWidget::clicked, this, &TextureViewer::on_render_clicked);
    QObject::connect(ui->render, &CustomPaintWidget::mouseMove, this, &TextureViewer::on_render_clicked);

	ui->dockarea->addToolWindow(ui->renderContainer, ToolWindowManager::EmptySpace);
	ui->dockarea->setToolWindowProperties(renderContainer, ToolWindowManager::DisallowUserDocking |
	                                                       ToolWindowManager::HideCloseButton |
	                                                       ToolWindowManager::DisableDraggableTab);

	ToolWindowManager::AreaReference ref(ToolWindowManager::AddTo, ui->dockarea->areaOf(renderContainer));

	/*
	QWidget *lockedTabTest = new QWidget(this);
	lockedTabTest->setWindowTitle(tr("Locked Tab #1"));

	ui->dockarea->addToolWindow(lockedTabTest, ref);
	ui->dockarea->setToolWindowProperties(lockedTabTest, ToolWindowManager::DisallowUserDocking | ToolWindowManager::HideCloseButton);

	lockedTabTest = new QWidget(this);
	lockedTabTest->setWindowTitle(tr("Locked Tab #2"));
	
	ui->dockarea->addToolWindow(lockedTabTest, ref);
	ui->dockarea->setToolWindowProperties(lockedTabTest, ToolWindowManager::DisallowUserDocking | ToolWindowManager::HideCloseButton);

	lockedTabTest = new QWidget(this);
	lockedTabTest->setWindowTitle(tr("Locked Tab #3"));
	
	ui->dockarea->addToolWindow(lockedTabTest, ref);
	ui->dockarea->setToolWindowProperties(lockedTabTest, ToolWindowManager::DisallowUserDocking | ToolWindowManager::HideCloseButton);

	lockedTabTest = new QWidget(this);
	lockedTabTest->setWindowTitle(tr("Locked Tab #4"));
	
	ui->dockarea->addToolWindow(lockedTabTest, ref);
	ui->dockarea->setToolWindowProperties(lockedTabTest, ToolWindowManager::DisallowUserDocking | ToolWindowManager::HideCloseButton);*/
	
	ui->dockarea->addToolWindow(ui->resourceThumbs, ToolWindowManager::AreaReference(ToolWindowManager::RightOf, ui->dockarea->areaOf(renderContainer)));
	ui->dockarea->setToolWindowProperties(ui->resourceThumbs, ToolWindowManager::HideCloseButton);
	
	ui->dockarea->addToolWindow(ui->targetThumbs, ToolWindowManager::AreaReference(ToolWindowManager::AddTo, ui->dockarea->areaOf(ui->resourceThumbs)));
	ui->dockarea->setToolWindowProperties(ui->targetThumbs, ToolWindowManager::HideCloseButton);
	
	// need to add a way to make this less than 50% programmatically
	ui->dockarea->addToolWindow(ui->pixelContextLayout, ToolWindowManager::AreaReference(ToolWindowManager::BottomOf, ui->dockarea->areaOf(ui->targetThumbs)));
	ui->dockarea->setToolWindowProperties(ui->pixelContextLayout, ToolWindowManager::HideCloseButton);
	
	ui->dockarea->setAllowFloatingWindow(false);
	ui->dockarea->setRubberBandLineWidth(50);

	renderContainer->setWindowTitle(tr("OM RenderTarget 0 - GBuffer Colour"));
	ui->pixelContextLayout->setWindowTitle(tr("Pixel Context"));
	ui->targetThumbs->setWindowTitle(tr("OM Targets"));
	ui->resourceThumbs->setWindowTitle(tr("PS Resources"));

	QVBoxLayout *vertical = new QVBoxLayout(this);

	vertical->setSpacing(3);
	vertical->setContentsMargins(0, 0, 0, 0);

	QWidget *flow1widget = new QWidget(this);
	QWidget *flow2widget = new QWidget(this);

	FlowLayout *flow1 = new FlowLayout(flow1widget, 0, 3, 3);
	FlowLayout *flow2 = new FlowLayout(flow2widget, 0, 3, 3);

	flow1widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
	flow2widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

	flow1->addWidget(ui->channelsToolbar);
	flow1->addWidget(ui->subresourceToolbar);
	flow1->addWidget(ui->actionToolbar);

	flow2->addWidget(ui->zoomToolbar);
	flow2->addWidget(ui->overlayToolbar);
	flow2->addWidget(ui->rangeToolbar);

	vertical->addWidget(flow1widget);
	vertical->addWidget(flow2widget);
	vertical->addWidget(ui->dockarea);

	Ui_TextureViewer *u = ui;
	u->pixelcontextgrid->setAlignment(u->pushButton, Qt::AlignCenter);
	u->pixelcontextgrid->setAlignment(u->pushButton_2, Qt::AlignCenter);
}

TextureViewer::~TextureViewer()
{
	m_Core->RemoveLogViewer(this);
	delete ui;
}

void TextureViewer::on_render_clicked(QMouseEvent *e)
{
    uint32_t x = (uint32_t)e->x();
    uint32_t y = (uint32_t)e->y();

    if(e->buttons() & Qt::RightButton)
    {
        m_Core->Renderer()->AsyncInvoke([this,x,y](IReplayRenderer *) {

            ResourceId id = m_TexDisplay.texid;

            PixelValue val;
            ReplayOutput_PickPixel(m_Output, id, false, x, y, 0, 0, 0, &val);

            QString str = QString("Pixel %1 %2 %3 %4").arg(val.value_f[0]).arg(val.value_f[1]).arg(val.value_f[2]).arg(val.value_f[3]);

            GUIInvoke::call([this,str,val]() {
                ui->statusText->setText(str);
                QPalette Pal(palette());

                // set black background
                Pal.setColor(QPalette::Background, QColor(int(val.value_f[0]*255), int(val.value_f[1]*255), int(val.value_f[2]*255)));
                ui->pickSwatch->setAutoFillBackground(true);
                ui->pickSwatch->setPalette(Pal);
            });
        });
    }
}

void TextureViewer::OnLogfileLoaded()
{
#if defined(WIN32)
	HWND wnd = (HWND)ui->render->winId();
#elif defined(__linux__)
	xcb_connection_t *display = QX11Info::connection();
	xcb_window_t window = (xcb_window_t)ui->render->winId();

	void *connectionScreenWindow[3] = { (void *)display, (void *)0, (void *)(uintptr_t)window };
	void *wnd = connectionScreenWindow;
#else
#error "Unknown platform"
#endif

	m_Core->Renderer()->BlockInvoke([wnd, this](IReplayRenderer *r) {
		m_Output = r->CreateOutput(wnd, eOutputType_TexDisplay);
		ui->render->SetOutput(m_Output);

		OutputConfig c = { eOutputType_TexDisplay };
		m_Output->SetOutputConfig(c);
	});
}

void TextureViewer::OnLogfileClosed()
{
	m_Output = NULL;
	ui->render->SetOutput(NULL);
}

void TextureViewer::OnEventSelected(uint32_t eventID)
{
	m_Core->Renderer()->AsyncInvoke([this](IReplayRenderer *) {

		TextureDisplay &d = m_TexDisplay;

		if(m_Core->APIProps().pipelineType == ePipelineState_D3D11)
		{
			d.texid = m_Core->CurD3D11PipelineState.m_OM.RenderTargets[0].Resource;
		}
		else if(m_Core->APIProps().pipelineType == ePipelineState_OpenGL)
		{
			d.texid = m_Core->CurGLPipelineState.m_FB.m_DrawFBO.Color[0].Obj;
		}
		else
		{
			const VulkanPipelineState &pipe = m_Core->CurVulkanPipelineState;
			if(pipe.Pass.renderpass.colorAttachments.count > 0)
				d.texid = pipe.Pass.framebuffer.attachments[pipe.Pass.renderpass.colorAttachments[0]].img;

			if(d.texid == ResourceId())
			{
				const FetchDrawcall *draw = m_Core->CurDrawcall();
				if(draw)
					d.texid = draw->copyDestination;
			}
		}
		d.mip = 0;
		d.sampleIdx = ~0U;
		d.overlay = eTexOverlay_None;
		d.CustomShader = ResourceId();
		d.HDRMul = -1.0f;
		d.linearDisplayAsGamma = true;
		d.FlipY = false;
		d.rangemin = 0.0f;
		d.rangemax = 1.0f;
		d.scale = -1.0f;
		d.offx = 0.0f;
		d.offy = 0.0f;
		d.sliceFace = 0;
		d.rawoutput = false;
		d.lightBackgroundColour = d.darkBackgroundColour =
			FloatVector(0.0f, 0.0f, 0.0f, 0.0f);
		d.Red = d.Green = d.Blue = true;
		d.Alpha = false;
		m_Output->SetTextureDisplay(d);

		FetchTexture *tex = m_Core->GetTexture(d.texid);
				
		GUIInvoke::call([this,tex]() {
			if(tex)
				ui->renderContainer->setWindowTitle(tr(tex->name.elems));

			ui->render->update();
		});
	});
}
