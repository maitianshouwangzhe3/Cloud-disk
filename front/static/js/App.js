// ... 顶部引入 ...
<div className="App">
  <div className="main-content">
    {/* 原有内容 */}
    <Routes>
      // ... 路由配置 ...
    </Routes>
  </div>
  
  {/* 新增备案组件 */}
  <div className="footer-beian">
    <a href="http://beian.miit.gov.cn" 
       target="_blank" 
       rel="noopener noreferrer"
       style={{ color: '#666', textDecoration: 'none' }}>
      粤ICP备2025394040号-1
    </a>
  </div>
</div>
