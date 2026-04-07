<script setup lang="ts">
/**
 * @file LLMConfig.vue
 * @brief LLM 配置管理组件
 */
import { ref, reactive, watch, inject, type Ref } from 'vue'
import type { LLMConfig, ApiResponse } from '../vite-env.d'

const showToast = inject<(msg: string, isError?: boolean) => void>('showToast')

const llmNames: Ref<string[]> = ref(['router', 'planner', 'executor', 'memory', 'image'])
const selectedLLM: Ref<string> = ref('router')
const llmConfigs = reactive<Record<string, LLMConfig>>({})
const llmConfig = reactive<LLMConfig>({
  apiKey: '', baseUrl: '', path: '', model: '',
  maxTokens: 100, temperature: 0.7, topP: 0.9
})
const saving: Ref<boolean> = ref(false)

// 首次加载所有配置
watch(selectedLLM, async (name: string) => {
  if (Object.keys(llmConfigs).length === 0) {
    const resp = await fetch('/admin/api/llm-configs')
    const data = await resp.json()
    Object.assign(llmConfigs, data)
  }
  if (llmConfigs[name]) {
    Object.assign(llmConfig, llmConfigs[name])
  }
}, { immediate: true })

const saveLLMConfig = async (): Promise<void> => {
  saving.value = true
  try {
    llmConfig.name = selectedLLM.value
    const resp = await fetch('/admin/api/llm-config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(llmConfig)
    })
    const data: ApiResponse = await resp.json()
    if (data.success) {
      showToast!('LLM配置已保存')
      llmConfigs[selectedLLM.value] = { ...llmConfig }
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
      <h1 class="page-title">LLM配置</h1>
      <p class="page-subtitle">配置各Agent的模型参数</p>
    </div>

    <div class="tabs">
      <button
        v-for="name in llmNames"
        :key="name"
        :class="{ active: selectedLLM === name }"
        @click="selectedLLM = name"
        class="tab"
      >{{ name }}</button>
    </div>

    <div class="card">
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">API Key</label>
          <input class="form-input" placeholder="sk-..." type="password" v-model="llmConfig.apiKey">
        </div>
        <div class="form-group">
          <label class="form-label">Base URL</label>
          <input class="form-input" placeholder="https://api.example.com" type="text" v-model="llmConfig.baseUrl">
        </div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">Path</label>
          <input class="form-input" placeholder="/v1/chat/completions" type="text" v-model="llmConfig.path">
        </div>
        <div class="form-group">
          <label class="form-label">Model</label>
          <input class="form-input" placeholder="gpt-4" type="text" v-model="llmConfig.model">
        </div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">Max Tokens</label>
          <input class="form-input" type="number" v-model.number="llmConfig.maxTokens">
        </div>
        <div class="form-group">
          <label class="form-label">Temperature</label>
          <input class="form-input" max="2" min="0" step="0.1" type="number" v-model.number="llmConfig.temperature">
        </div>
        <div class="form-group">
          <label class="form-label">Top P</label>
          <input class="form-input" max="1" min="0" step="0.1" type="number" v-model.number="llmConfig.topP">
        </div>
      </div>
      <button :disabled="saving" @click="saveLLMConfig" class="btn btn-primary">
        {{ saving ? '保存中...' : '保存配置' }}
      </button>
    </div>
  </div>
</template>