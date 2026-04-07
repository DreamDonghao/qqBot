<script setup lang="ts">
/**
 * @file KBConfig.vue
 * @brief 知识库配置组件
 */
import { reactive, onMounted, inject, ref, type Ref } from 'vue'
import type { KBConfig, ApiResponse } from '../vite-env.d'

const showToast = inject<(msg: string, isError?: boolean) => void>('showToast')

const kbConfig = reactive<KBConfig>({
  apiKey: '',
  baseUrl: '',
  knowledgeDatasetId: '',
  memoryDatasetId: '',
  memoryDocumentId: ''
})
const saving: Ref<boolean> = ref(false)

onMounted(async () => {
  const resp = await fetch('/admin/api/kb-config')
  const data = await resp.json()
  if (data.apiKey !== undefined) {
    Object.assign(kbConfig, data)
  }
})

const saveKBConfig = async (): Promise<void> => {
  saving.value = true
  try {
    const resp = await fetch('/admin/api/kb-config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(kbConfig)
    })
    const data: ApiResponse = await resp.json()
    if (data.success) {
      showToast!('知识库配置已保存')
    } else {
      showToast!(data.error || '保存失败', true)
    }
  } finally {
    saving.value = false
  }
}
</script>

<template>
  <div>
    <div class="page-header">
      <h1 class="page-title">知识库配置</h1>
      <p class="page-subtitle">配置RAGFlow知识库连接参数</p>
    </div>

    <div class="card">
      <div class="card-header">
        <h3 class="card-title">RAGFlow API 配置</h3>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">API Key</label>
          <input class="form-input" placeholder="ragflow-xxx" type="password" v-model="kbConfig.apiKey">
        </div>
        <div class="form-group">
          <label class="form-label">Base URL</label>
          <input class="form-input" placeholder="http://127.0.0.1:9380" type="text" v-model="kbConfig.baseUrl">
        </div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">知识库 Dataset ID</label>
          <input class="form-input" placeholder="Agent工具调用使用" type="text" v-model="kbConfig.knowledgeDatasetId">
        </div>
        <div class="form-group">
          <label class="form-label">记忆库 Dataset ID</label>
          <input class="form-input" placeholder="记忆存储使用" type="text" v-model="kbConfig.memoryDatasetId">
        </div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">记忆库 Document ID</label>
          <input class="form-input" placeholder="记忆文档ID（可选，不填则自动获取第一个）" type="text" v-model="kbConfig.memoryDocumentId">
        </div>
      </div>
      <button :disabled="saving" @click="saveKBConfig" class="btn btn-primary">
        {{ saving ? '保存中...' : '保存配置' }}
      </button>
    </div>
  </div>
</template>