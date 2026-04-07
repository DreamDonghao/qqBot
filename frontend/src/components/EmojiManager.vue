<script lang="ts" setup>
/**
 * @file EmojiManager.vue
 * @brief 表情库管理组件
 */
import {inject, onMounted, reactive, ref, type Ref} from 'vue'
import type {ApiResponse, Emoji} from '../vite-env.d'

const showToast = inject<(msg: string, isError?: boolean) => void>('showToast')

const emojis: Ref<Emoji[]> = ref([])
const newEmoji = reactive<Emoji>({name: '', path: ''})
const loading: Ref<boolean> = ref(false)
const saving: Ref<boolean> = ref(false)

const loadEmojis = async (): Promise<void> => {
  loading.value = true
  try {
    const resp = await fetch('/admin/api/emojis')
    emojis.value = await resp.json()
  } finally {
    loading.value = false
  }
}

const addEmoji = async (): Promise<void> => {
  if (!newEmoji.name || !newEmoji.path) {
    showToast!('请填写名称和路径', true)
    return
  }
  saving.value = true
  try {
    const resp = await fetch('/admin/api/emoji', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(newEmoji)
    })
    const data: ApiResponse = await resp.json()
    if (data.success) {
      showToast!('表情已添加')
      newEmoji.name = ''
      newEmoji.path = ''
      loadEmojis()
    } else {
      showToast!(data.error || '添加失败', true)
    }
  } finally {
    saving.value = false
  }
}

const removeEmoji = async (name: string): Promise<void> => {
  const resp = await fetch(`/admin/api/emoji/${encodeURIComponent(name)}`, {method: 'DELETE'})
  const data: ApiResponse = await resp.json()
  if (data.success) {
    showToast!('表情已删除')
    loadEmojis()
  }
}

onMounted(loadEmojis)
</script>

<template>
  <div>
    <div class="page-header">
      <h1 class="page-title">表情库</h1>
      <p class="page-subtitle">管理Bot可用的表情图片</p>
    </div>

    <div class="card">
      <div class="card-header">
        <h3 class="card-title">添加表情</h3>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label class="form-label">名称</label>
          <input v-model="newEmoji.name" class="form-input" placeholder="表情名称" type="text">
        </div>
        <div class="form-group">
          <label class="form-label">路径</label>
          <input v-model="newEmoji.path" class="form-input" placeholder="/path/to/emoji.png" type="text">
        </div>
      </div>
      <button :disabled="saving" class="btn btn-success" @click="addEmoji">
        {{ saving ? '添加中...' : '添加表情' }}
      </button>
    </div>

    <div class="card">
      <div class="card-header">
        <h3 class="card-title">表情列表</h3>
      </div>
      <div class="table-container">
        <table v-if="!loading">
          <thead>
          <tr>
            <th>名称</th>
            <th>路径</th>
            <th style="width:100px">操作</th>
          </tr>
          </thead>
          <tbody>
          <tr v-for="emoji in emojis" :key="emoji.name">
            <td>{{ emoji.name }}</td>
            <td><code style="font-size:12px">{{ emoji.path }}</code></td>
            <td>
              <button class="btn btn-danger btn-sm" @click="removeEmoji(emoji.name)">删除</button>
            </td>
          </tr>
          </tbody>
        </table>
        <div v-if="loading" class="empty-state">
          <p>加载中...</p>
        </div>
        <div v-else-if="emojis.length === 0" class="empty-state">
          <div class="empty-icon">😺</div>
          <p>暂无表情</p>
        </div>
      </div>
    </div>
  </div>
</template>