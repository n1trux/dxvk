#include "dxvk_hud_stats.h"

namespace dxvk::hud {
  
  HudStats::HudStats(HudElements elements)
  : m_elements(filterElements(elements)),
    m_compilerShowTime(dxvk::high_resolution_clock::now()) { }
  
  
  HudStats::~HudStats() {
    
  }
  
  
  void HudStats::update(const Rc<DxvkDevice>& device) {
    if (m_elements.isClear())
      return;
    
    // For some counters, we'll display the absolute value,
    // for others, the average counter increment per frame.
    DxvkStatCounters nextCounters = device->getStatCounters();
    m_diffCounters = nextCounters.diff(m_prevCounters);
    m_prevCounters = nextCounters;

    // GPU load is a bit more complex than that since
    // we don't want to update this every frame
    if (m_elements.test(HudElement::StatGpuLoad))
      this->updateGpuLoad();
  }
  
  
  HudPos HudStats::render(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    if (m_elements.test(HudElement::StatSubmissions))
      position = this->printSubmissionStats(context, renderer, position);
    
    if (m_elements.test(HudElement::StatDrawCalls))
      position = this->printDrawCallStats(context, renderer, position);
    
    if (m_elements.test(HudElement::StatPipelines))
      position = this->printPipelineStats(context, renderer, position);
    
    if (m_elements.test(HudElement::StatMemory))
      position = this->printMemoryStats(context, renderer, position);
    
    if (m_elements.test(HudElement::StatGpuLoad))
      position = this->printGpuLoad(context, renderer, position);
    
    if (m_elements.test(HudElement::CompilerActivity)) {
      this->printCompilerActivity(context, renderer,
        { position.x, float(renderer.surfaceSize().height) - 20.0f });
    }
    
    return position;
  }
  
  
  void HudStats::updateGpuLoad() {
    auto now = dxvk::high_resolution_clock::now();
    uint64_t ticks = std::chrono::duration_cast<std::chrono::microseconds>(now - m_gpuLoadUpdateTime).count();

    if (ticks >= 500'000) {
      m_gpuLoadUpdateTime = now;

      m_diffGpuIdleTicks = m_prevCounters.getCtr(DxvkStatCounter::GpuIdleTicks) - m_prevGpuIdleTicks;
      m_prevGpuIdleTicks = m_prevCounters.getCtr(DxvkStatCounter::GpuIdleTicks);

      uint64_t busyTicks = ticks > m_diffGpuIdleTicks
        ? uint64_t(ticks - m_diffGpuIdleTicks)
        : uint64_t(0);

      m_gpuLoadString = str::format("GPU: ", (100 * busyTicks) / ticks, "%");
    }
  }


  HudPos HudStats::printDrawCallStats(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    const uint64_t frameCount = std::max<uint64_t>(m_diffCounters.getCtr(DxvkStatCounter::QueuePresentCount), 1);
    
    const uint64_t gpCalls = m_diffCounters.getCtr(DxvkStatCounter::CmdDrawCalls)       / frameCount;
    const uint64_t cpCalls = m_diffCounters.getCtr(DxvkStatCounter::CmdDispatchCalls)   / frameCount;
    const uint64_t rpCalls = m_diffCounters.getCtr(DxvkStatCounter::CmdRenderPassCount) / frameCount;
    
    const std::string strDrawCalls      = str::format("Draw calls:     ", gpCalls);
    const std::string strDispatchCalls  = str::format("Dispatch calls: ", cpCalls);
    const std::string strRenderPasses   = str::format("Render passes:  ", rpCalls);
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      strDrawCalls);
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y + 20.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      strDispatchCalls);
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y + 40.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      strRenderPasses);
    
    return { position.x, position.y + 64 };
  }
  
  
  HudPos HudStats::printSubmissionStats(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    const uint64_t frameCount = std::max<uint64_t>(m_diffCounters.getCtr(DxvkStatCounter::QueuePresentCount), 1);
    const uint64_t numSubmits = m_diffCounters.getCtr(DxvkStatCounter::QueueSubmitCount) / frameCount;
    
    const std::string strSubmissions = str::format("Queue submissions: ", numSubmits);
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      strSubmissions);
    
    return { position.x, position.y + 24.0f };
  }
  
  
  HudPos HudStats::printPipelineStats(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    const uint64_t gpCount = m_prevCounters.getCtr(DxvkStatCounter::PipeCountGraphics);
    const uint64_t cpCount = m_prevCounters.getCtr(DxvkStatCounter::PipeCountCompute);
    
    const std::string strGpCount = str::format("Graphics pipelines: ", gpCount);
    const std::string strCpCount = str::format("Compute pipelines:  ", cpCount);
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      strGpCount);
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y + 20.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      strCpCount);
    
    return { position.x, position.y + 44.0f };
  }
  
  
  HudPos HudStats::printMemoryStats(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    constexpr uint64_t mib = 1024 * 1024;
    
    const uint64_t memAllocated = m_prevCounters.getCtr(DxvkStatCounter::MemoryAllocated);
    const uint64_t memUsed      = m_prevCounters.getCtr(DxvkStatCounter::MemoryUsed);
    
    const std::string strMemAllocated = str::format("Memory allocated: ", memAllocated / mib, " MB");
    const std::string strMemUsed      = str::format("Memory used:      ", memUsed      / mib, " MB");
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      strMemAllocated);
    
    renderer.drawText(context, 16.0f,
      { position.x, position.y + 20.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      strMemUsed);
    
    return { position.x, position.y + 44.0f };
  }


  HudPos HudStats::printGpuLoad(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_gpuLoadString);

    return { position.x, position.y + 24.0f };
  }


  HudPos HudStats::printCompilerActivity(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    auto now    = dxvk::high_resolution_clock::now();
    bool doShow = m_prevCounters.getCtr(DxvkStatCounter::PipeCompilerBusy);

    if (m_prevCounters.getCtr(DxvkStatCounter::PipeCompilerBusy)
     && m_diffCounters.getCtr(DxvkStatCounter::PipeCompilerBusy))
      m_compilerShowTime = now;
    
    if (!doShow) {
      doShow |= std::chrono::duration_cast<std::chrono::milliseconds>(now - m_compilerShowTime)
              < std::chrono::milliseconds(1000);
    }
    
    if (doShow) {
      renderer.drawText(context, 16.0f,
        { position.x, position.y },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        "Compiling shaders...");
    }
    
    return { position.x, position.y + 24.0f };
  }
  
  
  HudElements HudStats::filterElements(HudElements elements) {
    return elements & HudElements(
      HudElement::StatDrawCalls,
      HudElement::StatSubmissions,
      HudElement::StatPipelines,
      HudElement::StatMemory,
      HudElement::StatGpuLoad,
      HudElement::CompilerActivity);
  }
  
}
