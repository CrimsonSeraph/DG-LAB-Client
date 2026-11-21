// 静态资源 Worker 入口：所有请求交给 ASSETS 绑定，不在运行期做任何构建
export default {
  async fetch(request, env) {
    return env.ASSETS.fetch(request);
  },
};
