<script lang="ts" setup>
/**
 * @file MemoryConfig.vue
 * @brief 记忆配置组件
 */
import {inject, onMounted, reactive, ref, type Ref} from 'vue'
import type {ApiResponse, MemoryConfig} from '../vite-env.d'

const showToast = inject<(msg: string, isError?: boolean) => void>('showToast')

const memoryConfig = reactive<MemoryConfig>({
  memoryTriggerCount: 16,
  memoryChatRecordLimit: 18,
  shortTermMemoryMax: 15,
  shortTermMemoryLimit: 20,
  memoryMigrateCount: 5
})
const saving: Ref<boolean> = ref(false)

onMounted(async () => {
  const resp = await fetch('/admin/api/memory-config')
  const data = await resp.json()
  if (data.memoryTriggerCount !== undefined) {
    Object.assign(memoryConfig, data)
  }
})

const saveMemoryConfig = async (): Promise<void> => {
  saving.value = true
  try {
    const resp = await fetch('/admin/api/memory-config', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(memoryConfig)
    })
    const data: ApiResponse = await resp.json()
    if (data.success) {
      showToast!('记忆配置已保存')
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
      <h1 class="page-title">记忆配置</h1>
      <p class="page-subtitle">配置记忆系统参数</p>
    </div>

    <div class="card">
      <div class="card-header">
        <h3 class="card-title">参数设置</h3>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">记忆触发间隔</label>
          <input v-model="memoryConfig.memoryTriggerCount" class="form-input" type="number">
          <p class="form-hint">每 N 条消息触发记忆生成</p>
        </div>
        <div class="form-group">
          <label class="form-label">聊天记录保留数</label>
          <input v-model="memoryConfig.memoryChatRecordLimit" class="form-input" type="number">
          <p class="form-hint">保留的聊天记录数量</p>
        </div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">短期记忆上限</label>
          <input v-model="memoryConfig.shortTermMemoryMax" class="form-input" type="number">
          <p class="form-hint">超过此数量触发迁移</p>
        </div>
        <div class="form-group">
          <label class="form-label">合并保留上限</label>
          <input v-model="memoryConfig.shortTermMemoryLimit" class="form-input" type="number">
          <p class="form-hint">合并时保留的最大条数</p>
        </div>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">每次迁移条数</label>
          <input v-model="memoryConfig.memoryMigrateCount" class="form-input" type="number">
          <p class="form-hint">每次迁移到长期记忆的条数</p>
        </div>
      </div>
      <button :disabled="saving" class="btn btn-primary" @click="saveMemoryConfig">
        {{ saving ? '保存中...' : '保存配置' }}
      </button>
    </div>
  </div>
</template>